

#include <asn1c/asn_application.h>
#include <asn1c/der_encoder.h>

#include <osmocom/core/msgb.h>
#include <osmocom/rspro/RsproPDU.h>

#include "rspro_util.h"

#define ASN_ALLOC_COPY(out, in) \
do {						\
	if (in)	 {				\
		out = CALLOC(1, sizeof(*in));	\
		OSMO_ASSERT(out);		\
		memcpy(out, in, sizeof(*in));	\
	}					\
} while (0)


struct msgb *rspro_msgb_alloc(void)
{
	return msgb_alloc_headroom(1024, 8, "RSPRO");
}

/*! BER-Encode an RSPRO message into  msgb. 
 *  \param[in] pdu Structure describing RSPRO PDU. Is freed by this function on success
 *  \returns callee-allocated message buffer containing encoded RSPRO PDU; NULL on error.
 */
struct msgb *rspro_enc_msg(RsproPDU_t *pdu)
{
	struct msgb *msg = rspro_msgb_alloc();
	asn_enc_rval_t rval;

	if (!msg)
		return NULL;

	msg->l2h = msg->data;
	rval = der_encode_to_buffer(&asn_DEF_RsproPDU, pdu, msgb_data(msg), msgb_tailroom(msg));
	if (rval.encoded < 0) {
		fprintf(stderr, "Failed to encode %s\n", rval.failed_type->name);
		msgb_free(msg);
		return NULL;
	}
	msgb_put(msg, rval.encoded);

	ASN_STRUCT_FREE(asn_DEF_RsproPDU, pdu);

	return msg;
}

/* consumes 'msg' _if_ it is successful */
RsproPDU_t *rspro_dec_msg(struct msgb *msg)
{
	RsproPDU_t *pdu = NULL;
	asn_dec_rval_t rval;

	printf("decoding %s\n", msgb_hexdump(msg));
	rval = ber_decode(NULL, &asn_DEF_RsproPDU, (void **) &pdu, msgb_l2(msg), msgb_l2len(msg));
	if (rval.code != RC_OK) {
		fprintf(stderr, "Failed to decode: %d. Consumed %lu of %u bytes\n",
			rval.code, rval.consumed, msgb_length(msg));
		msgb_free(msg);
		return NULL;
	}

	msgb_free(msg);

	return pdu;
}

static void fill_comp_id(ComponentIdentity_t *out, const struct app_comp_id *in)
{
	out->type = in->type;
	OCTET_STRING_fromString(&out->name, in->name);
	OCTET_STRING_fromString(&out->software, in->software);
	OCTET_STRING_fromString(&out->swVersion, in->sw_version);
	if (strlen(in->hw_manufacturer))
		out->hwManufacturer = OCTET_STRING_new_fromBuf(&asn_DEF_ComponentName,
								in->hw_manufacturer, -1);
	if (strlen(in->hw_model))
		out->hwModel = OCTET_STRING_new_fromBuf(&asn_DEF_ComponentName, in->hw_model, -1);
	if (strlen(in->hw_serial_nr))
		out->hwSerialNr = OCTET_STRING_new_fromBuf(&asn_DEF_ComponentName, in->hw_serial_nr, -1);
	if (strlen(in->hw_version))
		out->hwVersion = OCTET_STRING_new_fromBuf(&asn_DEF_ComponentName, in->hw_version, -1);
	if (strlen(in->fw_version))
		out->fwVersion = OCTET_STRING_new_fromBuf(&asn_DEF_ComponentName, in->fw_version, -1);
}

static void fill_ip4_port(IpPort_t *out, uint32_t ip, uint16_t port)
{
	uint32_t ip_n = htonl(ip);
	out->ip.present = IpAddress_PR_ipv4;
	OCTET_STRING_fromBuf(&out->ip.choice.ipv4, (const char *) &ip_n, 4);
	out->port = htons(port);
}


RsproPDU_t *rspro_gen_ConnectBankReq(const struct app_comp_id *a_cid,
					uint16_t bank_id, uint16_t num_slots)
{
	RsproPDU_t *pdu = CALLOC(1, sizeof(*pdu));
	if (!pdu)
		return NULL;
	pdu->msg.present = RsproPDUchoice_PR_connectBankReq;
	fill_comp_id(&pdu->msg.choice.connectBankReq.identity, a_cid);
	pdu->msg.choice.connectBankReq.bankId = bank_id;
	pdu->msg.choice.connectBankReq.numberOfSlots = num_slots;

	return pdu;
}

RsproPDU_t *rspro_gen_ConnectClientReq(const struct app_comp_id *a_cid, const ClientSlot_t *client)
{
	RsproPDU_t *pdu = CALLOC(1, sizeof(*pdu));
	if (!pdu)
		return NULL;
	pdu->msg.present = RsproPDUchoice_PR_connectClientReq;
	fill_comp_id(&pdu->msg.choice.connectClientReq.identity, a_cid);
	ASN_ALLOC_COPY(pdu->msg.choice.connectClientReq.clientSlot, client);

	return pdu;
}

RsproPDU_t *rspro_gen_ConnectClientRes(const struct app_comp_id *a_cid, e_ResultCode res)
{
	RsproPDU_t *pdu = CALLOC(1, sizeof(*pdu));
	if (!pdu)
		return NULL;
	pdu->version = 2;
	pdu->tag = 2342;
	pdu->msg.present = RsproPDUchoice_PR_connectClientRes;
	fill_comp_id(&pdu->msg.choice.connectClientRes.identity, a_cid);
	pdu->msg.choice.connectClientRes.result = res;

	return pdu;
}

RsproPDU_t *rspro_gen_CreateMappingReq(const ClientSlot_t *client, const BankSlot_t *bank)
{
	RsproPDU_t *pdu = CALLOC(1, sizeof(*pdu));
	if (!pdu)
		return NULL;
	pdu->msg.present = RsproPDUchoice_PR_createMappingReq;
	pdu->msg.choice.createMappingReq.client = *client;
	pdu->msg.choice.createMappingReq.bank = *bank;

	return pdu;
}

RsproPDU_t *rspro_gen_ConfigClientReq(const ClientSlot_t *client, uint32_t ip, uint16_t port)
{
	RsproPDU_t *pdu = CALLOC(1, sizeof(*pdu));
	if (!pdu)
		return NULL;
	pdu->msg.present = RsproPDUchoice_PR_configClientReq;
	pdu->msg.choice.configClientReq.clientSlot = *client;
	fill_ip4_port(&pdu->msg.choice.configClientReq.bankd, ip, port);

	return pdu;
}

RsproPDU_t *rspro_gen_SetAtrReq(uint16_t client_id, uint16_t slot_nr, const uint8_t *atr,
				unsigned int atr_len)
{
	RsproPDU_t *pdu = CALLOC(1, sizeof(*pdu));
	if (!pdu)
		return NULL;
	pdu->msg.present = RsproPDUchoice_PR_setAtrReq;
	pdu->msg.choice.setAtrReq.slot.clientId = client_id;
	pdu->msg.choice.setAtrReq.slot.slotNr = slot_nr;
	OCTET_STRING_fromBuf(&pdu->msg.choice.setAtrReq.atr, (const char *)atr, atr_len);

	return pdu;
}

RsproPDU_t *rspro_gen_TpduModem2Card(const ClientSlot_t *client, const BankSlot_t *bank,
				     const uint8_t *tpdu, unsigned int tpdu_len)
{
	RsproPDU_t *pdu = CALLOC(1, sizeof(*pdu));
	if (!pdu)
		return NULL;
	pdu->msg.present = RsproPDUchoice_PR_tpduModemToCard;
	OSMO_ASSERT(client);
	pdu->msg.choice.tpduModemToCard.fromClientSlot = *client;
	OSMO_ASSERT(bank);
	pdu->msg.choice.tpduModemToCard.toBankSlot = *bank;
	/* TODO: flags? */
	OCTET_STRING_fromBuf(&pdu->msg.choice.tpduModemToCard.data, (const char *)tpdu, tpdu_len);

	return pdu;
}

RsproPDU_t *rspro_gen_TpduCard2Modem(const BankSlot_t *bank, const ClientSlot_t *client,
				     const uint8_t *tpdu, unsigned int tpdu_len)
{
	RsproPDU_t *pdu = CALLOC(1, sizeof(*pdu));
	if (!pdu)
		return NULL;
	pdu->msg.present = RsproPDUchoice_PR_tpduCardToModem;
	OSMO_ASSERT(bank);
	pdu->msg.choice.tpduCardToModem.fromBankSlot = *bank;
	OSMO_ASSERT(client)
	pdu->msg.choice.tpduCardToModem.toClientSlot = *client;
	/* TODO: flags? */
	OCTET_STRING_fromBuf(&pdu->msg.choice.tpduCardToModem.data, (const char *)tpdu, tpdu_len);

	return pdu;
}
