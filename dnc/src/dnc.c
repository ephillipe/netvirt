/*
 * Dynamic Network Directory Service
 * Copyright (C) 2009-2013
 * Nicolas J. Bouliane <nib@dynvpn.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 *
 */

#ifdef _WIN32
#include <winsock2.h>
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>

#include <dnds.h>
#include <logger.h>
#include <mbuf.h>
#include <netbus.h>

#include "dnc.h"
#include "session.h"

#ifdef _WIN32
	#define DNC_IP_FILE	"dnc.ip"
#else
	#define DNC_IP_FILE	"/etc/dnds/dnc.ip"
#endif

struct dnc_cfg *dnc_cfg;
struct session *master_session;
static int g_shutdown = 0;

static void on_input(netc_t *netc);
static void on_secure(netc_t *netc);
static void dispatch_op(struct session *session, DNDSMessage_t *msg);

static void tunnel_in(struct session* session)
{
	DNDSMessage_t *msg = NULL;
	size_t frame_size = 0;
	char framebuf[2000];

	if (session->state != SESSION_STATE_AUTHED)
		return;

	frame_size = tapcfg_read(session->tapcfg, framebuf, 2000);

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_ethernet);
	DNDSMessage_set_ethernet(msg, (uint8_t*)framebuf, frame_size);

	net_send_msg(session->netc, msg);
	DNDSMessage_set_ethernet(msg, NULL, 0);
	DNDSMessage_del(msg);
}

static void tunnel_out(struct session *session, DNDSMessage_t *msg)
{
	uint8_t *framebuf;
	size_t framebufsz;

	DNDSMessage_get_ethernet(msg, &framebuf, &framebufsz);
	tapcfg_write(session->tapcfg, framebuf, framebufsz);
}

void terminate(struct session *session)
{
	session->state = SESSION_STATE_DOWN;
	net_disconnect(session->netc);
	session->netc = NULL;
}

void transmit_netinfo_request(struct session *session)
{

	const char *hwaddr;
	int hwaddrlen;
	char ip_local[16];

//	inet_get_local_ip(ip_local, INET_ADDRSTRLEN);
	hwaddr = tapcfg_iface_get_hwaddr(session->tapcfg, &hwaddrlen);

	DNDSMessage_t *msg;

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_dnm);

	DNMessage_set_seqNumber(msg, 0);
	DNMessage_set_ackNumber(msg, 0);
	DNMessage_set_operation(msg, dnop_PR_netinfoRequest);

	NetinfoRequest_set_ipLocal(msg, ip_local); // Is it still usefull ?
	NetinfoRequest_set_macAddr(msg, (uint8_t*)hwaddr);

	net_send_msg(session->netc, msg);
	DNDSMessage_del(msg);

}

void transmit_prov_request(netc_t *netc)
{
	size_t nbyte;
	DNDSMessage_t *msg;

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_dnm);

	DNMessage_set_seqNumber(msg, 1);
	DNMessage_set_ackNumber(msg, 0);
	DNMessage_set_operation(msg, dnop_PR_provRequest);

	ProvRequest_set_provCode(msg, dnc_cfg->prov_code, strlen(dnc_cfg->prov_code));

	nbyte = net_send_msg(netc, msg);
	DNDSMessage_del(msg);
	if (nbyte == -1) {
		jlog(L_NOTICE, "dnc]> malformed message\n", nbyte);
		return;
	}
}

void transmit_register(netc_t *netc)
{
	X509_NAME *subj_ptr;
	char subj[256];
        size_t nbyte;
	struct session *session = (struct session *)netc->ext_ptr;

        DNDSMessage_t *msg;

        DNDSMessage_new(&msg);
        DNDSMessage_set_channel(msg, 0);
        DNDSMessage_set_pdu(msg, pdu_PR_dnm);

        DNMessage_set_seqNumber(msg, 1);
        DNMessage_set_ackNumber(msg, 0);
        DNMessage_set_operation(msg, dnop_PR_authRequest);

	subj_ptr = X509_get_subject_name(session->passport->certificate);
	X509_NAME_get_text_by_NID(subj_ptr, NID_commonName, subj, 256);

	jlog(L_NOTICE, "dnc]> CN=%s", subj);
        AuthRequest_set_certName(msg, subj, strlen(subj));

        nbyte = net_send_msg(netc, msg);
	DNDSMessage_del(msg);
        if (nbyte == -1) {
                jlog(L_NOTICE, "dnc]> malformed message: %d\n", nbyte);
                return;
        }
	session->state = SESSION_STATE_WAIT_ANSWER;
        return;
}

static void on_disconnect(netc_t *netc)
{
	jlog(L_NOTICE, "dnc]> disconnected...\n");

	struct session *session;
	netc_t *retry_netc = NULL;

	session = netc->ext_ptr;
	session->state = SESSION_STATE_DOWN;

	do {
		sleep(5);
		jlog(L_NOTICE, "dnc]> connection retry...\n");

		retry_netc = net_client(dnc_cfg->server_address,
		dnc_cfg->server_port, NET_PROTO_UDT, NET_SECURE_ADH,
		session->passport, on_disconnect, on_input, on_secure);

		if (retry_netc) {
			session->state = SESSION_STATE_NOT_AUTHED;
			session->netc = retry_netc;
			retry_netc->ext_ptr = session;
			return;
		}
	} while (!g_shutdown);
}

static void on_secure(netc_t *netc)
{
	struct session *session;
	session = netc->ext_ptr;

	jlog(L_NOTICE, "dnc]> connection secured");

	if (session->state == SESSION_STATE_NOT_AUTHED) {
		if (session->passport == NULL || dnc_cfg->prov_code != NULL) {
			jlog(L_NOTICE, "dnc]> Provisioning mode...");
			transmit_prov_request(netc);
		}
		else
			transmit_register(netc);
	}
}

static void on_input(netc_t *netc)
{
	DNDSMessage_t *msg;
	struct session *session;
	mbuf_t **mbuf_itr;
	pdu_PR pdu;

	mbuf_itr = &netc->queue_msg;
	session = netc->ext_ptr;

	while (*mbuf_itr != NULL) {
		msg = (DNDSMessage_t *)(*mbuf_itr)->ext_buf;
		DNDSMessage_get_pdu(msg, &pdu);

		switch (pdu) {
		case pdu_PR_dnm:
			dispatch_op(session, msg);
			break;

		case pdu_PR_ethernet:
			tunnel_out(session, msg);
			break;

		default: /* Invalid PDU */
			terminate(session);
			return;
		}
		mbuf_del(mbuf_itr, *mbuf_itr);
	}
}

static void op_netinfo_response(struct session *session, DNDSMessage_t *msg)
{
        char ipAddress[INET_ADDRSTRLEN];
	FILE *fp = NULL;

	fp = fopen(DNC_IP_FILE, "r");
	if (fp == NULL) {
		jlog(L_ERROR, "%s doesn't exist, reprovision your client", DNC_IP_FILE);
		return;
	}
	fscanf(fp, "%s", ipAddress);
	fclose(fp);

	tapcfg_iface_set_ipv4(session->tapcfg, ipAddress, 24);
	tapcfg_iface_set_status(session->tapcfg, TAPCFG_STATUS_IPV4_UP);
	session->state = SESSION_STATE_AUTHED;
}

static void op_auth_response(struct session *session, DNDSMessage_t *msg)
{
	e_DNDSResult result;
	AuthResponse_get_result(msg, &result);

	switch (result) {
	case DNDSResult_success:
		jlog(L_NOTICE, "dnc]> session authenticated");
		transmit_netinfo_request(session);
		break;

	case DNDSResult_secureStepUp:
		jlog(L_NOTICE, "dnc]> server authentication require step up");
		net_step_up(session->netc);
		break;

	default:
		jlog(L_NOTICE, "dnc]> unknown AuthResponse result (%i)", result);
	}
}

static void op_prov_response(struct session *session, DNDSMessage_t *msg)
{
	size_t length;
	char *certificate = NULL;
	char *certificatekey = NULL;
	char *trusted_authority = NULL;
        char ipAddress[INET_ADDRSTRLEN];
	FILE *fp = NULL;

	ProvResponse_get_certificate(msg, &certificate, &length);

	fp = fopen(dnc_cfg->certificate, "w");
	fwrite(certificate, 1, strlen(certificate), fp);
	fclose(fp);

	ProvResponse_get_certificateKey(msg, &certificatekey, &length);
	fp = fopen(dnc_cfg->privatekey, "w");
	fwrite(certificatekey, 1, strlen(certificatekey), fp);
	fclose(fp);

	ProvResponse_get_trustedCert(msg, &trusted_authority, &length);
	fp = fopen(dnc_cfg->trusted_cert, "w");
	fwrite(trusted_authority, 1, strlen(trusted_authority), fp);
	fclose(fp);

	ProvResponse_get_ipAddress(msg, ipAddress);
	printf("dnc]> ip address: %s\n", ipAddress);
	fp = fopen(DNC_IP_FILE, "w");
	fprintf(fp, "%s", ipAddress);
	fclose(fp);

	session->passport = pki_passport_load_from_file(dnc_cfg->certificate,
					 dnc_cfg->privatekey,
					 dnc_cfg->trusted_cert);

	krypt_add_passport(session->netc->kconn, session->passport);
	transmit_register(session->netc);
}

static void dispatch_op(struct session *session, DNDSMessage_t *msg)
{
	dnop_PR operation;
	DNMessage_get_operation(msg, &operation);

	switch (operation) {
	case dnop_PR_provResponse:
		op_prov_response(session, msg);
		break;

	case dnop_PR_authResponse:
		op_auth_response(session, msg);
		break;

	case dnop_PR_netinfoResponse:
		op_netinfo_response(session, msg);
		break;

	case dnop_PR_p2pRequest:
		// TODO
		break;

	/* `terminateRequest` is a special case since it has no
	 * response message associated with it, simply disconnect the client.
	 */
	case dnop_PR_NOTHING:
	default:
		jlog(L_NOTICE, "dnc]> not a valid DNM operation");
	case dnop_PR_terminateRequest:
		terminate(session);
		break;
	}
}

int dnc_init(struct dnc_cfg *cfg)
{
	struct session *session;

	dnc_cfg = cfg;
	session = calloc(1, sizeof(struct session));

	if (dnc_cfg->prov_code == NULL)
		session->passport = pki_passport_load_from_file(
			dnc_cfg->certificate, dnc_cfg->privatekey, dnc_cfg->trusted_cert);

	if (session->passport == NULL && dnc_cfg->prov_code == NULL) {
		jlog(L_ERROR, "dnc]> Must provide a provisioning code: ./dnc -p ...");
		return -1;
	}

	session->netc = net_client(dnc_cfg->server_address, dnc_cfg->server_port,
			NET_PROTO_UDT, NET_SECURE_ADH, session->passport,
			on_disconnect, on_input, on_secure);

	if (session->netc == NULL) {
		printf("netc is null\n");
		free(session);
		return -1;
	}

	session->tapcfg = NULL;
	session->state = SESSION_STATE_NOT_AUTHED;
	session->netc->ext_ptr = session;

	session->tapcfg = tapcfg_init();
	if (session->tapcfg == NULL) {
		jlog(L_ERROR, "dnc]> tapcfg_init failed");
		return -1;
	}

	if (tapcfg_start(session->tapcfg, NULL, 1) < 0) {
		jlog(L_ERROR, "dnc]> tapcfg_start failed");
		return -1;
	}

	session->devname = tapcfg_get_ifname(session->tapcfg);
	jlog(L_DEBUG, "dnc]> devname: %s", session->devname);

	while (!g_shutdown) {
		udtbus_poke_queue();
		if (tapcfg_wait_readable(session->tapcfg, 0))
			tunnel_in(session);
	}

	return 0;
}
