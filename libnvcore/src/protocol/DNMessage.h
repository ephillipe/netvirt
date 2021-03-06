/*
 * Generated by asn1c-0.9.27 (http://lionet.info/asn1c)
 * From ASN.1 module "DNDS"
 * 	found in "dnds.asn1"
 * 	`asn1c -fnative-types`
 */

#ifndef	_DNMessage_H_
#define	_DNMessage_H_


#include <asn_application.h>

/* Including external dependencies */
#include <NativeInteger.h>
#include "AuthRequest.h"
#include "AuthResponse.h"
#include "NetinfoRequest.h"
#include "NetinfoResponse.h"
#include "ProvRequest.h"
#include "ProvResponse.h"
#include "P2pRequest.h"
#include "P2pResponse.h"
#include "TerminateRequest.h"
#include <constr_CHOICE.h>
#include <constr_SEQUENCE.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Dependencies */
typedef enum dnop_PR {
	dnop_PR_NOTHING,	/* No components present */
	dnop_PR_authRequest,
	dnop_PR_authResponse,
	dnop_PR_netinfoRequest,
	dnop_PR_netinfoResponse,
	dnop_PR_provRequest,
	dnop_PR_provResponse,
	dnop_PR_p2pRequest,
	dnop_PR_p2pResponse,
	dnop_PR_terminateRequest,
	/* Extensions may appear below */
	
} dnop_PR;

/* DNMessage */
typedef struct DNMessage {
	unsigned long	 seqNumber;
	unsigned long	 ackNumber;
	struct dnop {
		dnop_PR present;
		union DNMessage__dnop_u {
			AuthRequest_t	 authRequest;
			AuthResponse_t	 authResponse;
			NetinfoRequest_t	 netinfoRequest;
			NetinfoResponse_t	 netinfoResponse;
			ProvRequest_t	 provRequest;
			ProvResponse_t	 provResponse;
			P2pRequest_t	 p2pRequest;
			P2pResponse_t	 p2pResponse;
			TerminateRequest_t	 terminateRequest;
			/*
			 * This type is extensible,
			 * possible extensions are below.
			 */
		} choice;
		
		/* Context for parsing across buffer boundaries */
		asn_struct_ctx_t _asn_ctx;
	} dnop;
	
	/* Context for parsing across buffer boundaries */
	asn_struct_ctx_t _asn_ctx;
} DNMessage_t;

/* Implementation */
/* extern asn_TYPE_descriptor_t asn_DEF_seqNumber_2;	// (Use -fall-defs-global to expose) */
/* extern asn_TYPE_descriptor_t asn_DEF_ackNumber_3;	// (Use -fall-defs-global to expose) */
extern asn_TYPE_descriptor_t asn_DEF_DNMessage;

#ifdef __cplusplus
}
#endif

#endif	/* _DNMessage_H_ */
#include <asn_internal.h>
