/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/ipc.h>

#include <mgba/internal/ds/ds.h>
#include <mgba/internal/ds/io.h>

void DSIPCWriteSYNC(struct ARMCore* remoteCpu, uint16_t* remoteIo, int16_t value) {
	remoteIo[DS_REG_IPCSYNC >> 1] &= 0xFFF0;
	remoteIo[DS_REG_IPCSYNC >> 1] |= (value >> 8) & 0x0F;
	if (value & 0x2000 && remoteIo[DS_REG_IPCSYNC >> 1] & 0x4000) {
		mLOG(DS_IO, STUB, "Unimplemented IPC IRQ");
		UNUSED(remoteCpu);
	}
}

int16_t DSIPCWriteFIFOCNT(struct DSCommon* dscore, int16_t value) {
	value &= 0xC40C;
	int16_t oldValue = dscore->memory.io[DS_REG_IPCFIFOCNT >> 1] & 0x4303;
	int16_t newValue = value | oldValue;
	if (DSIPCFIFOCNTIsError(value)) {
		newValue = DSIPCFIFOCNTClearError(0x8FFF);
	}
	if (DSIPCFIFOCNTIsSendClear(newValue)) {
		CircleBufferClear(&dscore->ipc->fifo);
	}
	return newValue;
}

void DSIPCWriteFIFO(struct DSCommon* dscore, int32_t value) {
	if (!DSIPCFIFOCNTIsEnable(dscore->memory.io[DS_REG_IPCFIFOCNT >> 1])) {
		return;
	}
	CircleBufferWrite32(&dscore->ipc->fifo, value);
	size_t fullness = CircleBufferSize(&dscore->ipc->fifo);
	if (fullness == 4) {
		dscore->memory.io[DS_REG_IPCFIFOCNT >> 1] = DSIPCFIFOCNTClearSendEmpty(dscore->memory.io[DS_REG_IPCFIFOCNT >> 1]);
		dscore->ipc->memory.io[DS_REG_IPCFIFOCNT >> 1] = DSIPCFIFOCNTClearRecvEmpty(dscore->ipc->memory.io[DS_REG_IPCFIFOCNT >> 1]);
		if (DSIPCFIFOCNTIsRecvIRQ(dscore->ipc->memory.io[DS_REG_IPCFIFOCNT >> 1])) {
			// TODO: Adaptive time slicing?
			DSRaiseIRQ(dscore->ipc->cpu, dscore->ipc->memory.io, DS_IRQ_IPC_NOT_EMPTY);
		}
	} else if (fullness == 64) {
		dscore->memory.io[DS_REG_IPCFIFOCNT >> 1] = DSIPCFIFOCNTFillSendFull(dscore->memory.io[DS_REG_IPCFIFOCNT >> 1]);
		dscore->ipc->memory.io[DS_REG_IPCFIFOCNT >> 1] = DSIPCFIFOCNTFillRecvFull(dscore->ipc->memory.io[DS_REG_IPCFIFOCNT >> 1]);
	}
}