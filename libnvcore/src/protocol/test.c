/*
 * NetVirt - Network Virtualization Platform
 * Copyright (C) 2009-2014
 * Nicolas J. Bouliane <admin@netvirt.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation; version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details
 */

/* API Dynamic-Network-Directory-Service-Protocol-V1 */

#include "asn_application.h"
#include "xer_encoder.h"
#include "dnds.h"

/* HOWTO compile `test' unit-test
 * gcc test.c -c -o test.o -I ../ -I ./
 * gcc ../dnds.o *.o -o test
 */

void hexDump (char *desc, void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;

    // Output description if given.
    if (desc != NULL)
        printf ("%s:\n", desc);

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                printf ("  %s\n", buff);

            // Output the offset.
            printf ("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf (" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII bit.
    printf ("  %s\n", buff);
}


#define ETH_ALEN 6
static int write_out(const void *buffer, size_t size, void *app_key)
{
	FILE *out_fp = app_key;
	size_t wrote;

	hexDump("", buffer, size);
	wrote = fwrite(buffer, 1, size, out_fp);

	return (wrote == size) ? 0 : -1;
}

DNDSMessage_t *decode()
{
	char buf[1024];

	DNDSMessage_t *msg = NULL;	// Type to Decode
	asn_dec_rval_t rval;
	FILE *fp;
	size_t size;
	char *filename = "dnds.ber";

	fp = fopen(filename, "rb");
	if (fp == NULL)
		exit(-1);

	size = fread(buf, 1, sizeof(buf), fp);
	fclose(fp);
	rval = ber_decode(0, &asn_DEF_DNDSMessage, (void **)&msg, buf, size);
	if (rval.code != RC_OK) {
		fprintf(stderr, "%s: broken dndsmessage encoding at byte %ld\n",
			filename, (long)rval.consumed);
			exit(-1);
	}

	int ret;
	char errbuf[128] = {0};
	size_t errlen = sizeof(errbuf);

	ret = asn_check_constraints(&asn_DEF_DNDSMessage, msg, errbuf, &errlen);
	printf("\ndecode ret:(%i) errbuf:(%s)\n", ret, errbuf);

	return msg;
}

void show_DNDS_ethernet()
{
	DNDSMessage_t *msg;
	msg = decode();
	DNDSMessage_printf(msg);
}

void test_DNDS_ethernet()
{
	/// Building an message ethernet frame ///

	DNDSMessage_t *msg;	// a DNDS Message

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_ethernet);	// ethernet frame

	uint8_t *frame = strdup("0110101010101");
	size_t frame_size = 13;

	DNDSMessage_set_ethernet(msg, frame, frame_size);

	/// Encoding part

	asn_enc_rval_t ec;	// Encoder return value
	FILE *fp = fopen("dnds.ber", "wb"); // BER output
	ec = der_encode(&asn_DEF_DNDSMessage, msg, write_out, fp);
	fclose(fp);

	xer_fprint(stdout, &asn_DEF_DNDSMessage, msg);

	// XXX free(frame)
	DNDSMessage_del(msg);
}

void show_AddRequest()
{
	DNDSMessage_t *msg;

	msg = decode();
	DNDSMessage_printf(msg);
	DSMessage_printf(msg);
	AddRequest_printf(msg);

	DNDSObject_t *obj;
	AddRequest_get_object(msg, &obj);
	DNDSObject_printf(obj);

	xer_fprint(stdout, &asn_DEF_DNDSMessage, msg);
}

void test_AddRequest()
{
	/// Building a AddRequest ///

	DNDSMessage_t *msg;		// a DNDS Message
	DNDSObject_t *objClient;	// a DS Object

	DNDSMessage_new(&msg);
//	DNDSMessage_set_version(msg, 1);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_dsm);	// Directory Service Message

	DSMessage_set_seqNumber(msg, 1);
	DSMessage_set_ackNumber(msg, 1);	// seq XOR ack
	DSMessage_set_operation(msg, dsop_PR_addRequest);
//	DSMessage_set_action(msg, action_addClient);

	AddRequest_set_objectType(msg, DNDSObject_PR_client, &objClient);

//	Client_set_id(objClient, 987);
	Client_set_email(objClient, "test@test", 9);
	Client_set_password(objClient, "test", 4);
//	Client_set_status(objClient, 0);

	/// Encoding part

	asn_enc_rval_t ec;	// Encoder return value
	FILE *fp = fopen("dnds.ber", "wb"); // BER output
	ec = der_encode(&asn_DEF_DNDSMessage, msg, write_out, fp);
	fclose(fp);

	xer_fprint(stdout, &asn_DEF_DNDSMessage, msg);

	asn_fprint(stdout, &asn_DEF_DNDSMessage, msg);


	DNDSMessage_del(msg);
}

void show_AddResponse()
{
	DNDSMessage_t *msg;

	msg = decode();
	DNDSMessage_printf(msg);
	DSMessage_printf(msg);
	AddResponse_printf(msg);
}

void test_AddResponse()
{
	/// Building a AddResponse ///

	DNDSMessage_t *msg;	// a DNDS Message

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_dsm);	// Directory Service Message

	DSMessage_set_seqNumber(msg, 0);
	DSMessage_set_ackNumber(msg, 4034);
	DSMessage_set_operation(msg, dsop_PR_addResponse);

	AddResponse_set_result(msg, DNDSResult_success);

	/// Encoding part

	asn_enc_rval_t ec;	// Encoder return value
	FILE *fp = fopen("dnds.ber", "wb"); // BER output
	ec = der_encode(&asn_DEF_DNDSMessage, msg, write_out, fp);
	fclose(fp);

	xer_fprint(stdout, &asn_DEF_DNDSMessage, msg);

	DNDSMessage_del(msg);
}

void show_SearchResponse_context()
{
	DNDSMessage_t *msg;

	msg = decode();
	DNDSMessage_printf(msg);
	DSMessage_printf(msg);
	SearchResponse_printf(msg);

	DNDSObject_t *obj;
	uint32_t count; int ret;

	SearchResponse_get_object_count(msg, &count);

	while (count-- > 0) {

		ret = SearchResponse_get_object(msg, &obj);
		if (ret == DNDS_success && obj != NULL) {
			DNDSObject_printf(obj);
		}
	}
}

void test_SearchResponse_context()
{
	/// Building a SearchResponse

	DNDSMessage_t *msg;	// A DNDS Message

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_dsm);

	DSMessage_set_seqNumber(msg, 0);
	DSMessage_set_ackNumber(msg, 400);
	DSMessage_set_operation(msg, dsop_PR_searchResponse);

	SearchResponse_set_result(msg, DNDSResult_success);

	DNDSObject_t *objContext;
	DNDSObject_new(&objContext);
	DNDSObject_set_objectType(objContext, DNDSObject_PR_context);

	Context_set_id(objContext, 10);
	Context_set_description(objContext, "home network", 12);
	Context_set_network(objContext, "44.128.0.0");
	Context_set_netmask(objContext, "255.255.0.0");
	Context_set_serverCert(objContext, "serverCert", 10);
	Context_set_serverPrivkey(objContext, "serverPrivkey", 13);
	Context_set_trustedCert(objContext, "trustedCert", 11);

	SearchResponse_add_object(msg, objContext);

	/// Encoding part

	asn_enc_rval_t ec;	// Encoder return value
	FILE *fp = fopen("dnds.ber", "wb"); // BER output
	ec = der_encode(&asn_DEF_DNDSMessage, msg, write_out, fp);
	fclose(fp);

	xer_fprint(stdout, &asn_DEF_DNDSMessage, msg);

	DNDSMessage_del(msg);

}

show_AddRequest_node()
{
	DNDSMessage_t *msg;

	msg = decode();
	DNDSMessage_printf(msg);
	DSMessage_printf(msg);
	AddRequest_printf(msg);

	DNDSObject_t *obj;
	AddRequest_get_object(msg, &obj);
	DNDSObject_printf(obj);

	xer_fprint(stdout, &asn_DEF_DNDSMessage, msg);
}

test_AddRequest_node()
{
	/// Building node AddRequest ///

	DNDSMessage_t *msg;
	DNDSObject_t *obj;

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_dsm);

	DSMessage_set_seqNumber(msg, 0);
	DSMessage_set_ackNumber(msg, 0);
	DSMessage_set_operation(msg, dsop_PR_addRequest);

	AddRequest_set_objectType(msg, DNDSObject_PR_node, &obj);

	Node_set_contextId(obj, 100);
	Node_set_description(obj, "voip node 1", 11);

	/// Encoding part

	asn_enc_rval_t ec;	// Encoder return value
	FILE *fp = fopen("dnds.ber", "wb"); // BER output
	ec = der_encode(&asn_DEF_DNDSMessage, msg, write_out, fp);
	fclose(fp);

	xer_fprint(stdout, &asn_DEF_DNDSMessage, msg);

	DNDSMessage_del(msg);
}

show_AddRequest_context()
{
	DNDSMessage_t *msg;

	msg = decode();
	DNDSMessage_printf(msg);
	DSMessage_printf(msg);
	AddRequest_printf(msg);

	DNDSObject_t *obj;
	AddRequest_get_object(msg, &obj);
	DNDSObject_printf(obj);

	xer_fprint(stdout, &asn_DEF_DNDSMessage, msg);
}

test_AddRequest_context()
{
	/// Building context AddRequest ///

	DNDSMessage_t *msg;
	DNDSObject_t *obj;

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_dsm);

	DSMessage_set_seqNumber(msg, 0);
	DSMessage_set_ackNumber(msg, 0);
	DSMessage_set_operation(msg, dsop_PR_addRequest);

	AddRequest_set_objectType(msg, DNDSObject_PR_context, &obj);

	Context_set_clientId(obj, 100);
	Context_set_description(obj, "home network1", 13);
	Context_set_network(obj, "44.128.0.0");
	Context_set_netmask(obj, "255.255.255.0");

	/// Encoding part

	asn_enc_rval_t ec;	// Encoder return value
	FILE *fp = fopen("dnds.ber", "wb"); // BER output
	ec = der_encode(&asn_DEF_DNDSMessage, msg, write_out, fp);
	fclose(fp);

	xer_fprint(stdout, &asn_DEF_DNDSMessage, msg);

	DNDSMessage_del(msg);
}

void show_SearchRequest_context()
{
	DNDSMessage_t *msg;

	msg = decode();
	DNDSMessage_printf(msg);
	DSMessage_printf(msg);
	SearchRequest_printf(msg);
}

void test_SearchRequest_context()
{
	/// Building a SearchRequest context ///
	int ret;

	DNDSMessage_t *msg;	// a DNDS Message

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_dsm);	// Directory Service Message

	DSMessage_set_seqNumber(msg, 800);
	DSMessage_set_ackNumber(msg, 0);
	DSMessage_set_operation(msg, dsop_PR_searchRequest);

	SearchRequest_set_searchType(msg, SearchType_all);
	SearchRequest_set_objectName(msg, ObjectName_context);

	/// Encoding part

	asn_enc_rval_t ec;	// Encoder return value
	FILE *fp = fopen("dnds.ber", "wb"); // BER output
	ec = der_encode(&asn_DEF_DNDSMessage, msg, write_out, fp);
	fclose(fp);

	xer_fprint(stdout, &asn_DEF_DNDSMessage, msg);

	DNDSMessage_del(msg);

}

void show_NodeConnectInfo()
{
	DNDSMessage_t *msg;

	msg = decode();
	DNDSMessage_printf(msg);
	DSMessage_printf(msg);
	//NodeConnectInfo_printf(msg);
}

void test_NodeConnectInfo()
{
	/// Building a NodeConnectInfo ///
	int ret;

	DNDSMessage_t *msg;

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_dsm);	// Directory Service Message

	DSMessage_set_seqNumber(msg, 800);
	DSMessage_set_ackNumber(msg, 0);
	//DSMessage_set_operation(msg, dsop_PR_nodeConnectInfo);

	//NodeConnectInfo_set_certName(msg, "unique_name@context", 19);
	//NodeConnectInfo_set_ipAddr(msg, "44.128.0.1");
	//NodeConnectInfo_set_state(msg, ConnectState_connected);

	/// Encoding part

	asn_enc_rval_t ec;	// Encoder return value
	FILE *fp = fopen("dnds.ber", "wb"); // BER output
	ec = der_encode(&asn_DEF_DNDSMessage, msg, write_out, fp);
	fclose(fp);

	xer_fprint(stdout, &asn_DEF_DNDSMessage, msg);

	DNDSMessage_del(msg);
}

void show_P2pRequest_dnm()
{
	DNDSMessage_t *msg;

	msg = decode();
	DNDSMessage_printf(msg);
	DNMessage_printf(msg);
	P2pRequest_printf(msg);
}

void test_P2pRequest_dnm()
{
	/// Building a P2pRequest ///
	int ret;
	uint8_t macAddrSrc[ETH_ALEN] = { 0xe6, 0x1b, 0x23, 0x0c, 0x0c, 0x5d };
	uint8_t macAddrDst[ETH_ALEN] = { 0xe6, 0x1b, 0x23, 0x0c, 0x0c, 0x5d };

	DNDSMessage_t *msg;	// a DNDS Message

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_dnm);	// Dynamic Network Message

	DNMessage_set_seqNumber(msg, 801);
	DNMessage_set_ackNumber(msg, 0);
	DNMessage_set_operation(msg, dnop_PR_p2pRequest);

	P2pRequest_set_ipAddrDst(msg, "66.55.44.33");
	P2pRequest_set_port(msg, 9000);
	P2pRequest_set_side(msg, P2pSide_client);
	P2pRequest_set_macAddrDst(msg, macAddrDst);

	/// Encoding part

	asn_enc_rval_t ec;	// Encoder return value
	FILE *fp = fopen("dnds.ber", "wb"); // BER output
	ec = der_encode(&asn_DEF_DNDSMessage, msg, write_out, fp);
	fclose(fp);

	xer_fprint(stdout, &asn_DEF_DNDSMessage, msg);

	DNDSMessage_del(msg);
}

void show_P2pResponse_dnm()
{
	DNDSMessage_t *msg;

	msg = decode();
	DNDSMessage_printf(msg);
	DNMessage_printf(msg);
	P2pResponse_printf(msg);
}

void test_P2pResponse_dnm()
{
	/// Building a P2pRequest ///

	uint8_t macAddrDst[ETH_ALEN] = { 0xaf, 0xbe, 0xcd, 0xdc, 0xeb, 0xfa };

	DNDSMessage_t *msg;	// a DNDS Message

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_dnm);	// Dynamic Network Message

	DNMessage_set_seqNumber(msg, 0);
	DNMessage_set_ackNumber(msg, 801);
	DNMessage_set_operation(msg, dnop_PR_p2pResponse);

	P2pResponse_set_macAddrDst(msg, macAddrDst);
	P2pResponse_set_result(msg, DNDSResult_success);

	/// Encoding part

	asn_enc_rval_t ec;	// Encoder return value
	FILE *fp = fopen("dnds.ber", "wb"); // BER output
	ec = der_encode(&asn_DEF_DNDSMessage, msg, write_out, fp);
	fclose(fp);

	xer_fprint(stdout, &asn_DEF_DNDSMessage, msg);

	DNDSMessage_del(msg);
}

void show_AuthRequest_dnm()
{
	DNDSMessage_t *msg;

	msg = decode();
	DNDSMessage_printf(msg);
	DNMessage_printf(msg);
	AuthRequest_printf(msg);
}

void test_AuthRequest_dnm()
{
	/// Building an AuthRequest ///

	DNDSMessage_t *msg;	// a DNDS Message

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_dnm);	// Dynamic Network Message

	DNMessage_set_seqNumber(msg, 100);
	DNMessage_set_ackNumber(msg, 0);
	DNMessage_set_operation(msg, dnop_PR_authRequest);

	AuthRequest_set_certName(msg, "nib@1", 5);

	/// Encoding part

	asn_enc_rval_t ec;	// Encoder return value
	FILE *fp = fopen("dnds.ber", "wb"); // BER output
	ec = der_encode(&asn_DEF_DNDSMessage, msg, write_out, fp);
	fclose(fp);

	xer_fprint(stdout, &asn_DEF_DNDSMessage, msg);

	DNDSMessage_del(msg);
}

void show_AuthResponse_dnm()
{
	DNDSMessage_t *msg;

	msg = decode();
	DNDSMessage_printf(msg);
	DNMessage_printf(msg);
	AuthResponse_printf(msg);
}

void test_AuthResponse_dnm()
{
	/// Building an AuthRequest ///

	DNDSMessage_t *msg;	// a DNDS Message

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_dnm);	// Dynamic Network Message

	DNMessage_set_seqNumber(msg, 0);
	DNMessage_set_ackNumber(msg, 100);
	DNMessage_set_operation(msg, dnop_PR_authResponse);

	AuthResponse_set_result(msg, DNDSResult_success);

	/// Encoding part

	asn_enc_rval_t ec;	// Encoder return value
	FILE *fp = fopen("dnds.ber", "wb"); // BER output
	ec = der_encode(&asn_DEF_DNDSMessage, msg, write_out, fp);
	fclose(fp);

	xer_fprint(stdout, &asn_DEF_DNDSMessage, msg);

	DNDSMessage_del(msg);
}

void show_AuthRequest()
{
	DNDSMessage_t *msg;

	msg = decode();
	DNDSMessage_printf(msg);
	DSMessage_printf(msg);
	AuthRequest_printf(msg);
}

void test_AuthRequest()
{
	/// Building an AuthRequest ///

	DNDSMessage_t *msg;	// a DNDS Message

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_dsm);	// Directory Service Message

	DSMessage_set_seqNumber(msg, 100);
	DSMessage_set_ackNumber(msg, 0);
	DSMessage_set_operation(msg, dnop_PR_authRequest);

	AuthRequest_set_certName(msg, "nib@1", 5);

	/// Encoding part

	asn_enc_rval_t ec;	// Encoder return value
	FILE *fp = fopen("dnds.ber", "wb"); // BER output
	ec = der_encode(&asn_DEF_DNDSMessage, msg, write_out, fp);
	fclose(fp);

	xer_fprint(stdout, &asn_DEF_DNDSMessage, msg);

	DNDSMessage_del(msg);
}

void show_AuthResponse()
{
	DNDSMessage_t *msg;

	msg = decode();
	DNDSMessage_printf(msg);
	DSMessage_printf(msg);
	AuthResponse_printf(msg);
}

void test_AuthResponse()
{
	/// Building an AuthResponse ///

	DNDSMessage_t *msg;	// a DNDS Message

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_dsm);	// Directory Service Message

	DSMessage_set_seqNumber(msg, 0);
	DSMessage_set_ackNumber(msg, 100);
	DSMessage_set_operation(msg, dnop_PR_authResponse);

	AuthResponse_set_result(msg, DNDSResult_success);

	/// Encoding part

	asn_enc_rval_t ec;	// Encoder return value
	FILE *fp = fopen("dnds.ber", "wb"); // BER output
	ec = der_encode(&asn_DEF_DNDSMessage, msg, write_out, fp);
	fclose(fp);

	xer_fprint(stdout, &asn_DEF_DNDSMessage, msg);

	DNDSMessage_del(msg);
}

void show_DelRequest()
{
	DNDSMessage_t *msg;

	msg = decode();
	DNDSMessage_printf(msg);
	DSMessage_printf(msg);
	DelRequest_printf(msg);

	DNDSObject_t *obj;
	DelRequest_get_object(msg, &obj);
	DNDSObject_printf(obj);
}

void test_DelRequest()
{
#if 0
	/// Building a DelRequest ///

	DNDSMessage_t *msg;	// a DNDS Message
	DNDSObject_t *objAcl;	// a DNDS Object

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_dsm);	// Directory Service Message

	DSMessage_set_seqNumber(msg, 200);
	DSMessage_set_ackNumber(msg, 0);
	DSMessage_set_operation(msg, dsop_PR_delRequest);

	DelRequest_set_objectType(msg, DNDSObject_PR_acl, &objAcl);

	Acl_set_id(objAcl, 1);
	Acl_set_contextId(objAcl, 2);
	Acl_set_description(objAcl, "une description", 15);

	/// Encoding part

	asn_enc_rval_t ec;	// Encoder return value
	FILE *fp = fopen("dnds.ber", "wb"); // BER output
	ec = der_encode(&asn_DEF_DNDSMessage, msg, write_out, fp);
	fclose(fp);

	xer_fprint(stdout, &asn_DEF_DNDSMessage, msg);

	DNDSMessage_del(msg);
#endif
}

void show_DelResponse()
{
	DNDSMessage_t *msg;

	msg = decode();
	DNDSMessage_printf(msg);
	DSMessage_printf(msg);
	DelResponse_printf(msg);
}

void test_DelResponse()
{
	/// Building a DelResponse ///

	DNDSMessage_t *msg;	// a DNDS Message

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_dsm);	// Directory Service Message

	DSMessage_set_seqNumber(msg, 0);
	DSMessage_set_ackNumber(msg, 200);
	DSMessage_set_operation(msg, dsop_PR_delResponse);

	DelResponse_set_result(msg, DNDSResult_success);

	/// Encoding part

	asn_enc_rval_t ec;	// Encoder return value
	FILE *fp = fopen("dnds.ber", "wb"); // BER output
	ec = der_encode(&asn_DEF_DNDSMessage, msg, write_out, fp);
	fclose(fp);

	xer_fprint(stdout, &asn_DEF_DNDSMessage, msg);

	DNDSMessage_del(msg);
}

void show_ModifyRequest()
{
	DNDSMessage_t *msg;

	msg = decode();
	DNDSMessage_printf(msg);
	DSMessage_printf(msg);
	ModifyRequest_printf(msg);

	DNDSObject_t *obj;
	ModifyRequest_get_object(msg, &obj);
	DNDSObject_printf(obj);
}

void test_ModifyRequest()
{
#if 0
	/// Building a ModifyRequest ///

	DNDSMessage_t *msg;		// a DNDS Message
	DNDSObject_t *objAclGroup;

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_dsm);

	DSMessage_set_seqNumber(msg, 300);
	DSMessage_set_ackNumber(msg, 0);
	DSMessage_set_operation(msg, dsop_PR_modifyRequest);

	ModifyRequest_set_objectType(msg, DNDSObject_PR_aclgroup, &objAclGroup);

	AclGroup_set_id(objAclGroup, 1);
	AclGroup_set_contextId(objAclGroup, 1);
	AclGroup_set_name(objAclGroup, "group-name", 10);
	AclGroup_set_description(objAclGroup, "a description", 13);

	/// Encoding part

	asn_enc_rval_t ec;	// Encoder return value
	FILE *fp = fopen("dnds.ber", "wb"); // BER output
	ec = der_encode(&asn_DEF_DNDSMessage, msg, write_out, fp);
	fclose(fp);

	xer_fprint(stdout, &asn_DEF_DNDSMessage, msg);

	DNDSMessage_del(msg);
#endif
}

void show_ModifyResponse()
{
	DNDSMessage_t *msg;

	msg = decode();
	DNDSMessage_printf(msg);
	DSMessage_printf(msg);
	ModifyResponse_printf(msg);
}

void test_ModifyResponse()
{
	/// Building a ModifyResponse ///

	DNDSMessage_t *msg;	// a DNDS Message

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_dsm);

	DSMessage_set_seqNumber(msg, 0);
	DSMessage_set_ackNumber(msg, 300);
	DSMessage_set_operation(msg, dsop_PR_modifyResponse);

	ModifyResponse_set_result(msg, DNDSResult_success);

	/// Encoding part

	asn_enc_rval_t ec;	// Encoder return value
	FILE *fp = fopen("dnds.ber", "wb"); // BER output
	ec = der_encode(&asn_DEF_DNDSMessage, msg, write_out, fp);
	fclose(fp);

	xer_fprint(stdout, &asn_DEF_DNDSMessage, msg);

	DNDSMessage_del(msg);
}

void show_NetinfoRequest()
{
	DNDSMessage_t *msg;

	msg = decode();
	DNDSMessage_printf(msg);
	DNMessage_printf(msg);
	NetinfoRequest_printf(msg);
}

void test_NetinfoRequest()
{
	/// Building a NetinfoRequest ///

	DNDSMessage_t *msg;	// a DNDS Message

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_dnm);

	DNMessage_set_seqNumber(msg, 600);
	DNMessage_set_ackNumber(msg, 0);
	DNMessage_set_operation(msg, dnop_PR_netinfoRequest);

	uint8_t macAddr[ETH_ALEN] = { 0xd, 0xe, 0xa, 0xd, 0xb, 0xe };

	NetinfoRequest_set_ipLocal(msg, "192.168.10.10");
	NetinfoRequest_set_macAddr(msg, macAddr);

	/// Encoding part

	asn_enc_rval_t ec;	// Encoder return value
	FILE *fp = fopen("dnds.ber", "wb"); // BER output
	ec = der_encode(&asn_DEF_DNDSMessage, msg, write_out, fp);
	fclose(fp);

	xer_fprint(stdout, &asn_DEF_DNDSMessage, msg);

	DNDSMessage_del(msg);
}

void show_NetinfoResponse()
{
	DNDSMessage_t *msg;

	msg = decode();
	DNDSMessage_printf(msg);
	DNMessage_printf(msg);
	NetinfoResponse_printf(msg);
}

void test_NetinfoResponse()
{
	/// Building a NetinfoResponse ///

	DNDSMessage_t *msg;	// a DNDS Message

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_dnm);

	DNMessage_set_seqNumber(msg, 0);
	DNMessage_set_ackNumber(msg, 600);
	DNMessage_set_operation(msg, dnop_PR_netinfoResponse);

	NetinfoResponse_set_ipAddress(msg, "192.168.10.5");
	NetinfoResponse_set_netmask(msg, "255.255.255.0");
	NetinfoResponse_set_result(msg, DNDSResult_success);

	/// Encoding part

	asn_enc_rval_t ec;	// Encoder return value
	FILE *fp = fopen("dnds.ber", "wb"); // BER output
	ec = der_encode(&asn_DEF_DNDSMessage, msg, write_out, fp);
	fclose(fp);

	xer_fprint(stdout, &asn_DEF_DNDSMessage, msg);

	DNDSMessage_del(msg);
}

void show_SearchRequest()
{
	DNDSMessage_t *msg;

	msg = decode();
	DNDSMessage_printf(msg);
	DSMessage_printf(msg);
	SearchRequest_printf(msg);

	DNDSObject_t *obj;
	SearchRequest_get_object(msg, &obj);
	DNDSObject_printf(obj);
}

void test_SearchRequest()
{
	/// Building a SearchRequest()

	DNDSMessage_t *msg;	// a DNDS Message
	DNDSObject_t *objNode; // a DNDS Object

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_dsm);

	DSMessage_set_seqNumber(msg, 400);
	DSMessage_set_ackNumber(msg, 0);
	DSMessage_set_operation(msg, dsop_PR_searchRequest);

	SearchRequest_set_searchType(msg, SearchType_object);


	DNDSObject_new(&objNode);
	DNDSObject_set_objectType(objNode, DNDSObject_PR_node);

	Node_set_contextId(objNode, 0);
	Node_set_provCode(objNode, "secret-prov-code", strlen("secret-prov-code"));
	Node_set_ipAddress(objNode, "127.0.0.1");

	SearchRequest_set_object(msg, objNode);

	/// Encoding part

	asn_enc_rval_t ec;	// Encoder return value
	FILE *fp = fopen("dnds.ber", "wb"); // BER output
	ec = der_encode(&asn_DEF_DNDSMessage, msg, write_out, fp);
	fclose(fp);

	xer_fprint(stdout, &asn_DEF_DNDSMessage, msg);

	DNDSMessage_del(msg);

}

void show_SearchResponse()
{
	DNDSMessage_t *msg;

	msg = decode();
	DNDSMessage_printf(msg);
	DSMessage_printf(msg);
	SearchResponse_printf(msg);

	DNDSObject_t *obj;
	uint32_t count; int ret;

	SearchResponse_get_object_count(msg, &count);

	while (count-- > 0) {

		ret = SearchResponse_get_object(msg, &obj);
		if (ret == DNDS_success && obj != NULL) {
			DNDSObject_printf(obj);
		}
	}
}

void test_SearchResponse()
{

	printf("TEST SEARCH RESPONSE\n");

	/// Building a SearchResponse

	DNDSMessage_t *msg;	// A DNDS Message

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_dsm);

	DSMessage_set_seqNumber(msg, 0);
	DSMessage_set_ackNumber(msg, 1);
	DSMessage_set_action(msg, action_listNode);
	DSMessage_set_operation(msg, dsop_PR_searchResponse);

	SearchResponse_set_result(msg, DNDSResult_success);
	SearchResponse_set_searchType(msg, SearchType_object);
/*
/// objContext
	DNDSObject_t *objContext;
	DNDSObject_new(&objContext);
	DNDSObject_set_objectType(objContext, DNDSObject_PR_context);

	Context_set_id(objContext, 40);
	SearchResponse_add_object(msg, objContext);
*/
/// Node
	DNDSObject_t *objNode;
	DNDSObject_new(&objNode);
	DNDSObject_set_objectType(objNode, DNDSObject_PR_node);

//	Node_set_contextId(objNode, 10);
	Node_set_description(objNode, "yo", strlen("yo"));
	Node_set_uuid(objNode, "abc", strlen("abc"));
	Node_set_certificate(objNode, "certificate", 11);
	Node_set_certificateKey(objNode, "key", 3);
	Node_set_status(objNode, 2);

	SearchResponse_add_object(msg, objNode);


	/// Encoding part

	asn_enc_rval_t ec;	// Encoder return value
	FILE *fp = fopen("dnds.ber", "wb"); // BER output
	ec = der_encode(&asn_DEF_DNDSMessage, msg, write_out, fp);
	fclose(fp);

	xer_fprint(stdout, &asn_DEF_DNDSMessage, msg);

	DNDSMessage_del(msg);
}

void test_TerminateRequest()
{
	/// Building a TerminateRequest ///

	DNDSMessage_t *msg;	// a DNDS Message

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_dsm);

	DSMessage_set_seqNumber(msg, 0);
	DSMessage_set_ackNumber(msg, 400);
	DSMessage_set_operation(msg, dsop_PR_terminateRequest);

	/// Encoding part

	asn_enc_rval_t ec;	// Encoder return value
	FILE *fp = fopen("dnds.ber", "wb"); // BER output
	ec = der_encode(&asn_DEF_DNDSMessage, msg, write_out, fp);
	fclose(fp);

	xer_fprint(stdout, &asn_DEF_DNDSMessage, msg);

	DNDSMessage_del(msg);
}

int main()
{

/*
	test_SearchRequest_context();
	show_SearchRequest_context();

	test_SearchResponse_context();
	show_SearchResponse_context();

	test_AddRequest_context();
	show_AddRequest_context();

	test_AddRequest_node();
	show_AddRequest_node();

	test_NodeConnectInfo();
	show_NodeConnectInfo();


	test_DNDS_ethernet();
	show_DNDS_ethernet();


	test_AddRequest();
	show_AddRequest();

	//test_AddResponse();
	//show_AddResponse();

	test_P2pRequest_dnm();
	show_P2pRequest_dnm();

	test_P2pResponse_dnm();
	show_P2pResponse_dnm();

	test_AuthRequest_dnm();
	show_AuthRequest_dnm();

	test_AuthResponse_dnm();
	show_AuthResponse_dnm();

	test_AuthRequest();
	show_AuthRequest();

	test_AuthResponse();
	show_AuthResponse();

	test_DelRequest();
	show_DelRequest();

	test_DelResponse();
	show_DelResponse();

	test_ModifyRequest();
	show_ModifyRequest();

	test_ModifyResponse();
	show_ModifyResponse();

	test_NetinfoRequest();
	show_NetinfoRequest();

	test_NetinfoResponse();
	show_NetinfoResponse();

	test_TerminateRequest();



	test_SearchRequest();
	show_SearchRequest();
*/
	test_SearchResponse();
	show_SearchResponse();
/*

*/

}
