
/*
 * Licensed Materials - Property of IBM
 *
 * trousers - An open source TCG Software Stack
 *
 * (C) Copyright International Business Machines Corp. 2004-2006
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "trousers/tss.h"
#include "trousers/trousers.h"
#include "spi_internal_types.h"
#include "spi_utils.h"
#include "capabilities.h"
#include "tsplog.h"
#include "hosttable.h"
#include "tcsd_wrap.h"
#include "obj.h"
#include "rpc_tcstp_tsp.h"


TSS_RESULT
TCSP_Seal_TP(struct host_table_entry *hte, TCS_CONTEXT_HANDLE hContext,	/* in */
			 TCS_KEY_HANDLE keyHandle,	/* in */
			 TCPA_ENCAUTH encAuth,	/* in */
			 UINT32 pcrInfoSize,	/* in */
			 BYTE * PcrInfo,	/* in */
			 UINT32 inDataSize,	/* in */
			 BYTE * inData,	/* in */
			 TPM_AUTH * pubAuth,	/* in, out */
			 UINT32 * SealedDataSize,	/* out */
			 BYTE ** SealedData	/* out */
    ) {
	TSS_RESULT result;
	int i = 0;

	initData(&hte->comm, 8);
	hte->comm.hdr.u.ordinal = TCSD_ORD_SEAL;
	LogDebugFn("TCS Context: 0x%x", hContext);

	if (setData(TCSD_PACKET_TYPE_UINT32, i++, &hContext, 0, &hte->comm))
		return TSPERR(TSS_E_INTERNAL_ERROR);
	if (setData(TCSD_PACKET_TYPE_UINT32, i++, &keyHandle, 0, &hte->comm))
		return TSPERR(TSS_E_INTERNAL_ERROR);
	if (setData(TCSD_PACKET_TYPE_ENCAUTH, i++, &encAuth, 0, &hte->comm))
		return TSPERR(TSS_E_INTERNAL_ERROR);
	if (setData(TCSD_PACKET_TYPE_UINT32, i++, &pcrInfoSize, 0, &hte->comm))
		return TSPERR(TSS_E_INTERNAL_ERROR);
	if (pcrInfoSize > 0) {
		if (setData(TCSD_PACKET_TYPE_PBYTE, i++, PcrInfo, pcrInfoSize, &hte->comm))
			return TSPERR(TSS_E_INTERNAL_ERROR);
	}
	if (setData(TCSD_PACKET_TYPE_UINT32, i++, &inDataSize, 0, &hte->comm))
		return TSPERR(TSS_E_INTERNAL_ERROR);
	if (inDataSize > 0) {
		if (setData(TCSD_PACKET_TYPE_PBYTE, i++, inData, inDataSize, &hte->comm))
			return TSPERR(TSS_E_INTERNAL_ERROR);
	}

	if (setData(TCSD_PACKET_TYPE_AUTH, i, pubAuth, 0, &hte->comm))
		return TSPERR(TSS_E_INTERNAL_ERROR);

	result = sendTCSDPacket(hte);

	if (result == TSS_SUCCESS)
		result = hte->comm.hdr.u.result;

	if (hte->comm.hdr.u.result == TSS_SUCCESS) {
		if (getData(TCSD_PACKET_TYPE_AUTH, 0, pubAuth, 0, &hte->comm)) {
			result = TSPERR(TSS_E_INTERNAL_ERROR);
			goto done;
		}

		if (getData(TCSD_PACKET_TYPE_UINT32, 1, SealedDataSize, 0, &hte->comm)) {
			result = TSPERR(TSS_E_INTERNAL_ERROR);
			goto done;
		}

		*SealedData = (BYTE *) malloc(*SealedDataSize);
		if (*SealedData == NULL) {
			LogError("malloc of %u bytes failed.", *SealedDataSize);
			result = TSPERR(TSS_E_OUTOFMEMORY);
			goto done;
		}
		if (getData(TCSD_PACKET_TYPE_PBYTE, 2, *SealedData, *SealedDataSize, &hte->comm)) {
			free(*SealedData);
			result = TSPERR(TSS_E_INTERNAL_ERROR);
		}
	}

done:
	return result;
}

TSS_RESULT
TCSP_Unseal_TP(struct host_table_entry *hte, TCS_CONTEXT_HANDLE hContext,	/* in */
			   TCS_KEY_HANDLE parentHandle,	/* in */
			   UINT32 SealedDataSize,	/* in */
			   BYTE * SealedData,	/* in */
			   TPM_AUTH * parentAuth,	/* in, out */
			   TPM_AUTH * dataAuth,	/* in, out */
			   UINT32 * DataSize,	/* out */
			   BYTE ** Data	/* out */
    ) {
	TSS_RESULT result;
	TSS_HCONTEXT tspContext;

	if ((tspContext = obj_lookupTspContext(hContext)) == NULL_HCONTEXT)
		return TSPERR(TSS_E_INTERNAL_ERROR);

	initData(&hte->comm, 6);
	hte->comm.hdr.u.ordinal = TCSD_ORD_UNSEAL;
	LogDebugFn("TCS Context: 0x%x", hContext);

	if (setData(TCSD_PACKET_TYPE_UINT32, 0, &hContext, 0, &hte->comm))
		return TSPERR(TSS_E_INTERNAL_ERROR);
	if (setData(TCSD_PACKET_TYPE_UINT32, 1, &parentHandle, 0, &hte->comm))
		return TSPERR(TSS_E_INTERNAL_ERROR);
	if (setData(TCSD_PACKET_TYPE_UINT32, 2, &SealedDataSize, 0, &hte->comm))
		return TSPERR(TSS_E_INTERNAL_ERROR);
	if (setData(TCSD_PACKET_TYPE_PBYTE, 3, SealedData, SealedDataSize, &hte->comm))
		return TSPERR(TSS_E_INTERNAL_ERROR);

	if (parentAuth != NULL) {
		if (setData(TCSD_PACKET_TYPE_AUTH, 4, parentAuth, 0, &hte->comm))
			return TSPERR(TSS_E_INTERNAL_ERROR);
	}

	if (setData(TCSD_PACKET_TYPE_AUTH, 5, dataAuth, 0, &hte->comm))
		return TSPERR(TSS_E_INTERNAL_ERROR);

	result = sendTCSDPacket(hte);

	if (result == TSS_SUCCESS)
		result = hte->comm.hdr.u.result;

	if (result == TSS_SUCCESS) {
		if (parentAuth != NULL) {
			if (getData(TCSD_PACKET_TYPE_AUTH, 0, parentAuth, 0, &hte->comm)) {
				result = TSPERR(TSS_E_INTERNAL_ERROR);
				goto done;
			}
		}

		if (getData(TCSD_PACKET_TYPE_AUTH, 1, dataAuth, 0, &hte->comm)) {
			result = TSPERR(TSS_E_INTERNAL_ERROR);
			goto done;
		}

		if (getData(TCSD_PACKET_TYPE_UINT32, 2, DataSize, 0, &hte->comm)) {
			result = TSPERR(TSS_E_INTERNAL_ERROR);
			goto done;
		}

		*Data = (BYTE *) calloc_tspi(tspContext, *DataSize);
		if (*Data == NULL) {
			LogError("malloc of %u bytes failed.", *DataSize);
			result = TSPERR(TSS_E_OUTOFMEMORY);
			goto done;
		}
		if (getData(TCSD_PACKET_TYPE_PBYTE, 3, *Data, *DataSize, &hte->comm)) {
			free_tspi(tspContext, *Data);
			result = TSPERR(TSS_E_INTERNAL_ERROR);
		}
	}

done:
	return result;
}
