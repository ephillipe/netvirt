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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dnds.h>
#include <logger.h>

#include "dao.h"
#include "ippool.h"
#include "pki.h"
#include "request.h"

void nodeConnectInfo(struct session *session, DNDSMessage_t *req_msg)
{
	(void)(session);

	size_t length;
	char *certName;
	char ipAddress[INET_ADDRSTRLEN];
	e_ConnState state;
	char *uuid = NULL;
	char *context_id = NULL;
	char *ptr = NULL;

	NodeConnInfo_get_certName(req_msg, &certName, &length);
	NodeConnInfo_get_ipAddr(req_msg, ipAddress);
	NodeConnInfo_get_state(req_msg, &state);

	uuid = strdup(certName+4);
	ptr = strchr(uuid, '@');
	*ptr = '\0';
	context_id = strdup(ptr+1);

	switch(state) {
	case ConnState_connected:
		dao_update_node_status(context_id, uuid, "1", ipAddress);
		break;
	case ConnState_disconnected:
		dao_update_node_status(context_id, uuid, "0", ipAddress);
		break;
	default:
		jlog(L_WARNING, "the connection state is invalid");
		break;
	}

	free(uuid);
	free(context_id);

	return;
}

void AddRequest_context(DNDSMessage_t *msg)
{
	(void)msg;
}

void DelRequest_context(DNDSMessage_t *msg)
{
	DNDSObject_t *object;
	DelRequest_get_object(msg, &object);

	int ret = 0;
	uint32_t contextId = -1;
	uint32_t clientId = 0;

	char context_id_str[10] = {0};
	char client_id_str[10] = {0};

	Context_get_id(object, &contextId);
	snprintf(context_id_str, 10, "%d", contextId);

	Context_get_clientId(object, &clientId);
	snprintf(client_id_str, 10, "%d", clientId);

	jlog(L_NOTICE, "deleting nodes belonging to context: %s", context_id_str);
	ret = dao_del_node_by_context_id(context_id_str);
	if (ret < 0) {
		jlog(L_NOTICE, "deleting nodes failed!");
		return;
	}

	jlog(L_NOTICE, "deleting context: %s", context_id_str);
	ret = dao_del_context(client_id_str, context_id_str);
	if (ret < 0) {
		/* FIXME: multiple DAO calls should be commited to the DB once
		 * all calls have succeeded.
		 */
		jlog(L_NOTICE, "deleting context failed!");
		return;
	}

	/* forward the delRequest to nvswitch */
	if (g_switch_netc)
		net_send_msg(g_switch_netc, msg);
}

void addNode(struct session *session, DNDSMessage_t *req_msg)
{
	(void)session;

	DNDSObject_t *obj;
	AddRequest_get_object(req_msg, &obj);

	int ret = 0;
	size_t length = 0;

	char *context_id = NULL;
	char *client_id = NULL;
	char *context_name = NULL;
	char *description = NULL;

	int exp_delay;
	embassy_t *emb = NULL;
	char *emb_cert_ptr = NULL;
	char *emb_pvkey_ptr = NULL;
	char *serial = NULL;
	unsigned char *ippool_bin = NULL;
	long size;

	char *apikey = NULL;
	char *uuid = NULL;
	char *provcode = NULL;
	char common_name[256] = {0};
	char *node_cert_ptr = NULL;
	char *node_pvkey_ptr = NULL;
	char emb_serial[10];

	DNDSMessage_t *resp_msg;

	/* Prepare response */
	DNDSMessage_new(&resp_msg);
	DNDSMessage_set_pdu(resp_msg, pdu_PR_dsm);

	DSMessage_set_action(resp_msg, action_addNode);
	DSMessage_set_operation(resp_msg, dsop_PR_addResponse);
	/* */

	Node_get_contextName(obj, &context_name, &length);
	Node_get_description(obj, &description, &length);

	ret = DSMessage_get_apikey(req_msg, &apikey, &length);
	if (ret != DNDS_success) {
		AddResponse_set_result(resp_msg, DNDSResult_noRight);
		goto out;
	}

	ret = dao_fetch_client_id_by_apikey(&client_id, apikey);
	printf("client id: %s\n", client_id);
	if (ret != 0) {
		AddResponse_set_result(resp_msg, DNDSResult_noRight);
		goto out;
	}

	ret = dao_fetch_network_id(&context_id, client_id, context_name);
	printf("context id: %s\n", context_id);
	if (ret != 0) {
		AddResponse_set_result(resp_msg, DNDSResult_noSuchObject);
		goto out;
	}

	exp_delay = pki_expiration_delay(10);
	ret = dao_fetch_context_embassy(context_id, &emb_cert_ptr, &emb_pvkey_ptr, &serial, &ippool_bin);
	jlog(L_DEBUG, "serial: %s", serial);
	if (ret != 0) {
		jlog(L_ERROR, "failed to fetch context embassy");
		AddResponse_set_result(resp_msg, DNDSResult_operationError);
		goto out;
	}

	emb = pki_embassy_load_from_memory(emb_cert_ptr, emb_pvkey_ptr, atoi(serial));

	uuid = uuid_v4();
	provcode = uuid_v4();

	printf("context id: %s\n", context_id);
	snprintf(common_name, sizeof(common_name), "nva-%s@%s", uuid, context_id);
	jlog(L_DEBUG, "common_name: %s", common_name);

	digital_id_t *node_ident = NULL;
	node_ident = pki_digital_id(common_name, "", "", "", "admin@netvirt.org", "NetVirt");

	passport_t *node_passport = NULL;
	node_passport = pki_embassy_deliver_passport(emb, node_ident, exp_delay);

	/* FIXME verify is the value is freed or not via BIO_free() */
	pki_write_certificate_in_mem(node_passport->certificate, &node_cert_ptr, &size);
	pki_write_privatekey_in_mem(node_passport->keyring, &node_pvkey_ptr, &size);

	snprintf(emb_serial, sizeof(emb_serial), "%d", emb->serial);

	ret = dao_update_embassy_serial(context_id, emb_serial);
	if (ret == -1) {
		jlog(L_ERROR, "failed to update embassy serial");
		AddResponse_set_result(resp_msg, DNDSResult_operationError);
		goto free1;
	}

	/* handle ip pool */
	struct ippool *ippool;
	char *ip;
	int pool_size;

	ippool = ippool_new("44.128.0.0", "255.255.0.0");
	free(ippool->pool);
	ippool->pool = (uint8_t*)ippool_bin;
	pool_size = (ippool->hosts+7)/8 * sizeof(uint8_t);
	ip = ippool_get_ip(ippool);

	ret = dao_add_node(context_id, uuid, node_cert_ptr, node_pvkey_ptr, provcode, description, ip);
	if (ret == -1) {
		jlog(L_ERROR, "failed to add node");
		AddResponse_set_result(resp_msg, DNDSResult_operationError);
		goto free2;
	}

	ret = dao_update_context_ippool(context_id, ippool->pool, pool_size);
	if (ret == -1) {
		jlog(L_ERROR, "failed to update embassy ippool");
		AddResponse_set_result(resp_msg, DNDSResult_operationError);
		goto free2;
	}


	/* send node update to nvswitch */

	DNDSMessage_t *msg_up;
	DNDSMessage_new(&msg_up);
	DNDSMessage_set_pdu(msg_up, pdu_PR_dsm);
	DSMessage_set_action(msg_up, action_addNode);
	DSMessage_set_operation(msg_up, dsop_PR_searchResponse);
	SearchResponse_set_searchType(msg_up, SearchType_sequence);
	SearchResponse_set_result(msg_up, DNDSResult_success);

	DNDSObject_t *objNode;
	DNDSObject_new(&objNode);
	DNDSObject_set_objectType(objNode, DNDSObject_PR_node);

	Node_set_uuid(objNode, uuid, strlen(uuid));
	Node_set_contextId(objNode, atoi(context_id));

	SearchResponse_add_object(msg_up, objNode);

	if (g_switch_netc)
		net_send_msg(g_switch_netc, msg_up);

	DNDSMessage_del(msg_up);

	AddResponse_set_result(resp_msg, DNDSResult_success);

free2:
	ippool_free(ippool);

free1:
	free(node_passport);
	pki_free_digital_id(node_ident);

	free(uuid);
	free(provcode);

	free(node_cert_ptr);
	free(node_pvkey_ptr);

	free(serial);
	free(emb);
	free(emb_cert_ptr);
	free(emb_pvkey_ptr);

out:
	net_send_msg(session->netc, resp_msg);
	DNDSMessage_del(resp_msg);
}

void DelRequest_node(DNDSMessage_t *msg)
{
	int ret = 0;

	DNDSObject_t *object;
	DelRequest_get_object(msg, &object);

	size_t length = 0;
	uint32_t contextId = -1;
	char context_id_str[10] = {0};
	Node_get_contextId(object, &contextId);
	snprintf(context_id_str, 10, "%d", contextId);

	char *uuid = NULL;
	Node_get_uuid(object, &uuid, &length);

	char *ipaddr = NULL;
	ret = dao_fetch_node_ip(context_id_str, uuid, &ipaddr);
	if (ret == -1) {
		jlog(L_ERROR, "failed to fetch node ip");
		return;
	}

	jlog(L_NOTICE, "revoking node: %s", uuid);
	dao_del_node(context_id_str, uuid);

	unsigned char *ippool_bin = NULL;
	ret = dao_fetch_context_ippool(context_id_str, &ippool_bin);
	if (ret == -1) {
		jlog(L_ERROR, "failed to fetch context ippool");
		return;
	}

	/* update ip pool */
	struct ippool *ippool;
	int pool_size;

	ippool = ippool_new("44.128.0.0", "255.255.0.0");
	free(ippool->pool);
	ippool->pool = (uint8_t*)ippool_bin;
	pool_size = (ippool->hosts+7)/8 * sizeof(uint8_t);
	ippool_release_ip(ippool, ipaddr);

	ret = dao_update_context_ippool(context_id_str, ippool->pool, pool_size);
	if (ret == -1) {
		jlog(L_ERROR, "failed to update embassy ippool");
	}

	/* forward the delRequest to nvswitch */
	if (g_switch_netc)
		net_send_msg(g_switch_netc, msg);
}

#if 0
void addRequest(DNDSMessage_t *msg)
{
	DNDSObject_PR objType;
	AddRequest_get_objectType(msg, &objType);

	if (objType == DNDSObject_PR_context) {
		AddRequest_context(msg);
	}

	if (objType == DNDSObject_PR_node) {
		AddRequest_node(msg);
	}
}
#endif

void delRequest(struct session *session, DNDSMessage_t *msg)
{
	(void)session;
	DNDSObject_PR objType;
	DelRequest_get_objectType(msg, &objType);

	if (objType == DNDSObject_PR_client) {
		/* FIXME : DelRequest_client(msg); */
	}

	if (objType == DNDSObject_PR_context) {
		DelRequest_context(msg);
	}

	if (objType == DNDSObject_PR_node) {
		DelRequest_node(msg);
	}
}

void modifyRequest(struct session *session, DNDSMessage_t *msg)
{
	(void)session;
	(void)msg;
}

void searchRequest_client(struct session *session, DNDSMessage_t *req_msg)
{
	DNDSObject_t *object;

	SearchRequest_get_object(req_msg, &object);

	size_t length;
	char *id = NULL;

	char *email;
	char *password;

	Client_get_email(object, &email, &length);
	Client_get_password(object, &password, &length);

	dao_fetch_client_id(&id, email, password);
	jlog(L_DEBUG, "client id: %s", id);

	DNDSMessage_t *msg;

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_dsm);

	DSMessage_set_seqNumber(msg, 0);
	DSMessage_set_ackNumber(msg, 1);
	DSMessage_set_action(msg, action_getAccountApiKey);
	DSMessage_set_operation(msg, dsop_PR_searchResponse);

	SearchResponse_set_searchType(msg, SearchType_object);
	SearchResponse_set_result(msg, DNDSResult_success);

	DNDSObject_t *objClient;
	DNDSObject_new(&objClient);
	DNDSObject_set_objectType(objClient, DNDSObject_PR_client);

	Client_set_id(objClient, id ? atoi(id): 0); /* FIXME set the result to failed if id == NULL */

	SearchResponse_add_object(msg, objClient);
	net_send_msg(session->netc, msg);
	DNDSMessage_del(msg);
	free(id);
}

void CB_searchRequest_context(void *data, int remaining,
				char *id,
				char *description,
				char *client_id,
				char *network,
				char *netmask,
				char *serverCert,
				char *serverPrivkey,
				char *trustedCert)
{
	(void)(client_id);
	static DNDSMessage_t *msg = NULL;
	static int count = 0;
	struct session *session = (struct session *)data;

	count++;

	if (msg == NULL) {

		DNDSMessage_new(&msg);
		DNDSMessage_set_channel(msg, 0);
		DNDSMessage_set_pdu(msg, pdu_PR_dsm);

		DSMessage_set_seqNumber(msg, 0);
		DSMessage_set_ackNumber(msg, 1);
		DSMessage_set_action(msg, action_listNetwork);
		DSMessage_set_operation(msg, dsop_PR_searchResponse);

		SearchResponse_set_searchType(msg, SearchType_all);
	}

	DNDSObject_t *objContext;
	DNDSObject_new(&objContext);
	DNDSObject_set_objectType(objContext, DNDSObject_PR_context);

	Context_set_id(objContext, atoi(id));
	Context_set_description(objContext, description, strlen(description));
	Context_set_network(objContext, network);
	Context_set_netmask(objContext, netmask);
	Context_set_serverCert(objContext, serverCert, strlen(serverCert));
	Context_set_serverPrivkey(objContext, serverPrivkey, strlen(serverPrivkey));
	Context_set_trustedCert(objContext, trustedCert, strlen(trustedCert));

	SearchResponse_add_object(msg, objContext);

	if (count == 10 || remaining == 0) {

		if (remaining == 0) {
			SearchResponse_set_result(msg, DNDSResult_success);
		} else {
			SearchResponse_set_result(msg, DNDSResult_moreData);
		}

		net_send_msg(session->netc, msg);
		DNDSMessage_del(msg);
		msg = NULL;

		count = 0;
	}
}

void searchRequest_context(struct session *session)
{
	int ret = 0;
	static DNDSMessage_t *msg = NULL;

	ret = dao_fetch_context(session, CB_searchRequest_context);
	if (ret == -1) {
		DNDSMessage_new(&msg);
		DNDSMessage_set_channel(msg, 0);
		DNDSMessage_set_pdu(msg, pdu_PR_dsm);

		DSMessage_set_seqNumber(msg, 0);
		DSMessage_set_ackNumber(msg, 1);
		DSMessage_set_action(msg, action_listNetwork);
		DSMessage_set_operation(msg, dsop_PR_searchResponse);

		// XXX should fail
		SearchResponse_set_searchType(msg, SearchType_all);

		SearchResponse_set_result(msg, DNDSResult_success);
		net_send_msg(session->netc, msg);
		DNDSMessage_del(msg);
	}
}

void CB_searchRequest_node_sequence(void *data, int remaining, char *uuid, char *context_id)
{
	static DNDSMessage_t *msg = NULL;
	static int count = 0;
	struct session *session = (struct session *)data;

	count++;

	if (msg == NULL) {

		DNDSMessage_new(&msg);
		DNDSMessage_set_pdu(msg, pdu_PR_dsm);
		DSMessage_set_action(msg, action_listNode);
		DSMessage_set_operation(msg, dsop_PR_searchResponse);
		SearchResponse_set_searchType(msg, SearchType_sequence);
	}

	DNDSObject_t *objNode;
	DNDSObject_new(&objNode);
	DNDSObject_set_objectType(objNode, DNDSObject_PR_node);

	Node_set_uuid(objNode, uuid, strlen(uuid));
	Node_set_contextId(objNode, atoi(context_id));

	SearchResponse_add_object(msg, objNode);

	if (count == 10 || remaining == 0) {

		if (remaining == 0) {
			SearchResponse_set_result(msg, DNDSResult_success);
		} else {
			SearchResponse_set_result(msg, DNDSResult_moreData);
		}

		net_send_msg(session->netc, msg);
		DNDSMessage_del(msg);

		msg = NULL;
		count = 0;
	}
}

void searchRequest_node_sequence(struct session *session, DNDSMessage_t *req_msg)
{
	/* extract the list of node context id from the req_msg */
	DNDSObject_t *object = NULL;
	static DNDSMessage_t *msg = NULL;
	uint32_t count = 0;
	uint32_t i = 0;
	uint32_t *id_list = NULL;
	uint32_t contextId = 0;
	int ret = 0;

	SearchRequest_get_object_count(req_msg, &count);
	if (count == 0) {
		/* XXX send failed reply */
		return;
	}

	id_list = calloc(count, sizeof(uint32_t));
	for (i = 0; i < count; i++) {

		ret = SearchRequest_get_from_objects(req_msg, &object);
		if (ret == DNDS_success && object != NULL) {
			Node_get_contextId(object, &contextId);
			id_list[i] = contextId;
			DNDSObject_del(object);
			object = NULL;
		}
		else {
			/* XXX send failed reply */
		}
	}

	/* sql query that return all the node uuid */
	ret = dao_fetch_node_sequence(id_list, count, session, CB_searchRequest_node_sequence);
	if (ret == -1) {
		DNDSMessage_new(&msg);
		DNDSMessage_set_pdu(msg, pdu_PR_dsm);
		DSMessage_set_action(msg, action_listNode);
		DSMessage_set_operation(msg, dsop_PR_searchResponse);
		SearchResponse_set_searchType(msg, SearchType_sequence);

		SearchResponse_set_result(msg, DNDSResult_success);
		net_send_msg(session->netc, msg);
		DNDSMessage_del(msg);
	}

	free(id_list);
}

int CB_searchRequest_node_by_context_id(void *msg, char *uuid, char *description, char *provcode, char *ipaddress, char *status)
{
	DNDSObject_t *objNode;
	DNDSObject_new(&objNode);
	DNDSObject_set_objectType(objNode, DNDSObject_PR_node);

	Node_set_description(objNode, description, strlen(description));
	Node_set_uuid(objNode, uuid, strlen(uuid));
	Node_set_provCode(objNode, provcode, strlen(provcode));
	Node_set_ipAddress(objNode, ipaddress);
	Node_set_status(objNode, atoi(status));

	SearchResponse_set_searchType(msg, SearchType_object);
	SearchResponse_add_object(msg, objNode);

	return 0;
}

void searchRequest_node(struct session *session, DNDSMessage_t *req_msg)
{
	char *provcode = NULL;
	uint32_t contextid = 0;
	char str_contextid[20];
	size_t length;
	int ret = 0;

	DNDSObject_t *obj = NULL;
        SearchRequest_get_object(req_msg, &obj);
	Node_get_provCode(obj, &provcode, &length);
	Node_get_contextId(obj, &contextid);

	uint32_t tracked_id;
	DSMessage_get_seqNumber(req_msg, &tracked_id);


	DNDSMessage_t *msg;

	DNDSMessage_new(&msg);
	DNDSMessage_set_channel(msg, 0);
	DNDSMessage_set_pdu(msg, pdu_PR_dsm);

	DSMessage_set_seqNumber(msg, tracked_id);
	DSMessage_set_ackNumber(msg, 0);
	DSMessage_set_action(msg, action_listNode);
	DSMessage_set_operation(msg, dsop_PR_searchResponse);

	if (contextid > 0) { /* searching by context ID */

		jlog(L_DEBUG, "context ID to search: %d", contextid);
		snprintf(str_contextid, sizeof(str_contextid), "%d", contextid);

		ret = dao_fetch_node_from_context_id(str_contextid, msg,
					CB_searchRequest_node_by_context_id);
		if (ret != 0) {
			jlog(L_WARNING, "dao fetch node from context id failed: %d", contextid);
			return; /* FIXME send negative response */
		}

		/* the fields are set via the callback */

	} else if (provcode != NULL) { /* searching by provcode */

		jlog(L_DEBUG, "searchRequest node for provisioning");

		DNDSObject_t *objNode;
		DNDSObject_new(&objNode);
		DNDSObject_set_objectType(objNode, DNDSObject_PR_node);

		jlog(L_DEBUG, "provcode to search: %s", provcode);

		char *certificate = NULL;
		char *private_key = NULL;
		char *trustedcert = NULL;
		char *ipAddress = NULL;

		ret = dao_fetch_node_from_provcode(provcode, &certificate, &private_key, &trustedcert, &ipAddress);
		if (ret != 0) {
			SearchResponse_set_result(msg, DNDSResult_noSuchObject);
			SearchResponse_set_searchType(msg, SearchType_object);
			net_send_msg(session->netc, msg);
			DNDSMessage_del(msg);
			return;
		}

		Node_set_certificate(objNode, certificate, strlen(certificate));
		Node_set_certificateKey(objNode, (uint8_t*)private_key, strlen(private_key));
		Node_set_trustedCert(objNode, (uint8_t*)trustedcert, strlen(trustedcert));
		Node_set_ipAddress(objNode, ipAddress);

		SearchResponse_set_result(msg, DNDSResult_success);
		SearchResponse_add_object(msg, objNode);
	}

	SearchResponse_set_searchType(msg, SearchType_object);
	net_send_msg(session->netc, msg);
	DNDSMessage_del(msg);
}

void addAccount(struct session *session, DNDSMessage_t *req_msg)
{
	jlog(L_DEBUG, "Add new account");

	DNDSMessage_t *resp_msg;
	DNDSObject_t *obj;
	AddRequest_get_object(req_msg, &obj);

	int ret = 0;
	size_t length = 0;
	char *email = NULL;
	char *password = NULL;
	char *apikey = NULL;

	/* Prepare response */
	DNDSMessage_new(&resp_msg);
	DNDSMessage_set_pdu(resp_msg, pdu_PR_dsm);

	DSMessage_set_action(resp_msg, action_addAccount);
	DSMessage_set_operation(resp_msg, dsop_PR_addResponse);
	/* */

	Client_get_password(obj, &password, &length);
	Client_get_email(obj, &email, &length);

	apikey = pki_gen_api_key();
	if (apikey == NULL) {
		AddResponse_set_result(resp_msg, DNDSResult_operationError);
		goto fail;
	}

	ret = dao_add_client(email, password, apikey);
	if (ret == -1) {
		AddResponse_set_result(resp_msg, DNDSResult_duplicate);
		goto fail;
	}


	AddResponse_set_result(resp_msg, DNDSResult_success);
fail:
	net_send_msg(session->netc, resp_msg);
	DNDSMessage_del(resp_msg);

	return;
}

int CB_listNetwork(void *msg, char *description)
{
	DNDSObject_t *objContext;
	DNDSObject_new(&objContext);
	DNDSObject_set_objectType(objContext, DNDSObject_PR_context);

	Context_set_description(objContext, description, strlen(description));

	SearchResponse_add_object(msg, objContext);

	return 0;
}

void listNetwork(struct session *session, DNDSMessage_t *req_msg)
{
	int ret = 0;
	char *client_id = NULL;
	size_t length = 0;
	char *apikey = NULL;

	/* Prepare response */
	DNDSMessage_t *resp_msg = NULL;
	DNDSMessage_new(&resp_msg);
	DNDSMessage_set_channel(resp_msg, 0);
	DNDSMessage_set_pdu(resp_msg, pdu_PR_dsm);

	DSMessage_set_seqNumber(resp_msg, 0);
	DSMessage_set_ackNumber(resp_msg, 1);
	DSMessage_set_action(resp_msg, action_listNetwork);
	DSMessage_set_operation(resp_msg, dsop_PR_searchResponse);

	SearchResponse_set_searchType(resp_msg, SearchType_object);
	/* */

	ret = DSMessage_get_apikey(req_msg, &apikey, &length);
	if (ret != DNDS_success) {
		SearchResponse_set_result(resp_msg, DNDSResult_noRight);
		goto out;
	}

	ret = dao_fetch_client_id_by_apikey(&client_id, apikey);
	if (ret == -1) {
		SearchResponse_set_result(resp_msg, DNDSResult_operationError);
		goto out;
	}
	if (client_id == NULL) {
		SearchResponse_set_result(resp_msg, DNDSResult_noRight);
		goto out;
	}

	ret = dao_fetch_networks_by_client_id(client_id, resp_msg, CB_listNetwork);
	SearchResponse_set_result(resp_msg, DNDSResult_success);
out:

	net_send_msg(session->netc, resp_msg);
	DNDSMessage_del(resp_msg);

	return;

}

int CB_fetch_network_by_client_id_desc(void *msg,
					char *id,
					char *description,
					char *client_id,
					char *network,
					char *netmask,
					char *serverCert,
					char *serverPrivkey,
					char *trustedCert)
{
	DNDSObject_t *objContext;
	DNDSObject_new(&objContext);
	DNDSObject_set_objectType(objContext, DNDSObject_PR_context);

	Context_set_id(objContext, atoi(id));
	Context_set_clientId(objContext, atoi(client_id));
	Context_set_description(objContext, description, strlen(description));
	Context_set_network(objContext, network);
	Context_set_netmask(objContext, netmask);

	Context_set_serverCert(objContext, serverCert, strlen(serverCert));
	Context_set_serverPrivkey(objContext, serverPrivkey, strlen(serverPrivkey));
	Context_set_trustedCert(objContext, trustedCert, strlen(trustedCert));

	SearchResponse_add_object(msg, objContext);

	return 0;
}

void addNetwork(struct session *session, DNDSMessage_t *req_msg)
{
	int ret = 0;
	char *client_id = NULL;
	size_t length;

	DNDSObject_t *obj;
	char *apikey = NULL;
	char *description = NULL;
	char network[INET_ADDRSTRLEN];
	char netmask[INET_ADDRSTRLEN];

	/* Prepare response */
	DNDSMessage_t *resp_msg = NULL;
	DNDSMessage_new(&resp_msg);
	DNDSMessage_set_pdu(resp_msg, pdu_PR_dsm);
	DSMessage_set_action(resp_msg, action_addNetwork);
	DSMessage_set_operation(resp_msg, dsop_PR_addResponse);
	/* */

	AddRequest_get_object(req_msg, &obj);
	DSMessage_get_apikey(req_msg, &apikey, &length);
	Context_get_description(obj, &description, &length);
	Context_get_network(obj, network);
	Context_get_netmask(obj, netmask);

	ret = dao_fetch_client_id_by_apikey(&client_id, apikey);
	if (ret == -1) {
		AddResponse_set_result(resp_msg, DNDSResult_operationError);
		goto out;
	}
	if (client_id == NULL) {
		AddResponse_set_result(resp_msg, DNDSResult_noRight);
		goto out;
	}

	/* 3.1- Initialise embassy */
	int exp_delay;
	exp_delay = pki_expiration_delay(10);

	digital_id_t *embassy_id;
	embassy_id = pki_digital_id("embassy", "CA", "Quebec", "", "admin@netvirt.org", "NetVirt");

	embassy_t *emb;
	emb = pki_embassy_new(embassy_id, exp_delay);
	pki_free_digital_id(embassy_id);

	char *emb_cert_ptr; long size;
	char *emb_pvkey_ptr;

	pki_write_certificate_in_mem(emb->certificate, &emb_cert_ptr, &size);
	pki_write_privatekey_in_mem(emb->keyring, &emb_pvkey_ptr, &size);

	/* 3.2- Initialize server passport */

	digital_id_t *server_id;
	server_id = pki_digital_id("nvswitch", "CA", "Quebec", "", "admin@netvirt.org", "NetVirt");

	passport_t *nvswitch_passport;
	nvswitch_passport = pki_embassy_deliver_passport(emb, server_id, exp_delay);
	pki_free_digital_id(server_id);

	char *serv_cert_ptr;
	char *serv_pvkey_ptr;

	pki_write_certificate_in_mem(nvswitch_passport->certificate, &serv_cert_ptr, &size);
	pki_write_privatekey_in_mem(nvswitch_passport->keyring, &serv_pvkey_ptr, &size);
	free(nvswitch_passport);

	char emb_serial[10];
	snprintf(emb_serial, sizeof(emb_serial), "%d", emb->serial);
	free(emb);

	/* Create an IP pool */
	struct ippool *ippool;
	size_t pool_size;

	ippool = ippool_new("44.128.0.0", "255.255.0.0");
	pool_size = (ippool->hosts+7)/8 * sizeof(uint8_t);

	ret = dao_add_context(client_id,
				description,
				"44.128.0.0/16",
				emb_cert_ptr,
				emb_pvkey_ptr,
				emb_serial,
				serv_cert_ptr,
				serv_pvkey_ptr,
				ippool->pool,
				pool_size);

	if (ret == -1) {
		AddResponse_set_result(resp_msg, DNDSResult_operationError);
		goto out;
	}

	if (ret == -2) {
		AddResponse_set_result(resp_msg, DNDSResult_duplicate);
		goto out;
	}

	ippool_free(ippool);

	free(serv_cert_ptr);
	free(serv_pvkey_ptr);

	free(emb_cert_ptr);
	free(emb_pvkey_ptr);

	/* send context update to nvswitch */
	DNDSMessage_t *msg_up;
	DNDSMessage_new(&msg_up);
	DNDSMessage_set_channel(msg_up, 0);
	DNDSMessage_set_pdu(msg_up, pdu_PR_dsm);

	DSMessage_set_seqNumber(msg_up, 0);
	DSMessage_set_ackNumber(msg_up, 1);
	DSMessage_set_action(msg_up, action_addNetwork);
	DSMessage_set_operation(msg_up, dsop_PR_searchResponse);

	dao_fetch_network_by_client_id_desc(client_id, description, msg_up,
		CB_fetch_network_by_client_id_desc);

	SearchResponse_set_searchType(msg_up, SearchType_all);
	SearchResponse_set_result(msg_up, DNDSResult_success);

	if (g_switch_netc) {
		net_send_msg(g_switch_netc, msg_up);
	}
	DNDSMessage_del(msg_up);
	/* */

	free(client_id);

	AddResponse_set_result(resp_msg, DNDSResult_success);
out:
	net_send_msg(session->netc, resp_msg);
	DNDSMessage_del(resp_msg);
}

void getAccountApiKey(struct session *session, DNDSMessage_t *req_msg)
{
	int ret = 0;
	size_t length = 0;
	char *email = NULL;
	char *password = NULL;
	char *apikey = NULL;

	/* Prepare response */
	DNDSMessage_t *resp_msg;

	DNDSMessage_new(&resp_msg);
	DNDSMessage_set_channel(resp_msg, 0);
	DNDSMessage_set_pdu(resp_msg, pdu_PR_dsm);

	DSMessage_set_seqNumber(resp_msg, 0);
	DSMessage_set_ackNumber(resp_msg, 0);
	DSMessage_set_action(resp_msg, action_getAccountApiKey);
	DSMessage_set_operation(resp_msg, dsop_PR_searchResponse);
	/* */

	DNDSObject_t *object;
	SearchRequest_get_object(req_msg, &object);

	Client_get_email(object, &email, &length);
	Client_get_password(object, &password, &length);

	ret = dao_fetch_account_apikey(&apikey, email, password);
	if (ret == -1) {
		SearchResponse_set_result(resp_msg, DNDSResult_operationError);
		jlog(L_DEBUG, "dao_fetch_account_apikey failed");
		goto out;
	}

	if (apikey == NULL) {
		SearchResponse_set_result(resp_msg, DNDSResult_noRight);
		jlog(L_DEBUG, "apikey is NULL");
		goto out;
	}

	DNDSObject_t *objClient;
	DNDSObject_new(&objClient);
	DNDSObject_set_objectType(objClient, DNDSObject_PR_client);

	Client_set_apikey(objClient, apikey, strlen(apikey));

	SearchResponse_set_result(resp_msg, DNDSResult_success);
	SearchResponse_add_object(resp_msg, objClient);
	SearchResponse_set_searchType(resp_msg, SearchType_object);

out:
	net_send_msg(session->netc, resp_msg);
	DNDSMessage_del(resp_msg);
}

void searchRequest(struct session *session, DNDSMessage_t *req_msg)
{
	e_SearchType SearchType;
	DNDSObject_t *object;
	DNDSObject_PR objType;

	SearchRequest_get_searchType(req_msg, &SearchType);

	SearchRequest_get_object(req_msg, &object);
	DNDSObject_get_objectType(object, &objType);

	if (SearchType == SearchType_all) {
		searchRequest_context(session);
	}

	if (SearchType == SearchType_sequence) {
		searchRequest_node_sequence(session, req_msg);
	}

	if (SearchType == SearchType_object) {

		switch (objType) {
		case DNDSObject_PR_client:
			searchRequest_client(session, req_msg);
			break;
		case DNDSObject_PR_node:
			searchRequest_node(session, req_msg);
			break;
		case DNDSObject_PR_context:
#if 0
			searchRequest_context_by_client_id(session, req_msg);
			break;
#endif
		case DNDSObject_PR_NOTHING:
			break;
		}
	}
}
