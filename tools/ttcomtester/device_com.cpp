
#include <stdarg.h>
#include <stdio.h>
#include <windows.h>
#include <winioctl.h>
#include <assert.h>

#include "ttlib.h"

#include "deviceope.h"

typedef struct comdata_st {
	wchar_t *port_name;
	HANDLE h;
	OVERLAPPED rol;
	OVERLAPPED wol;
	COMMPROP device_prop;
	bool dcb_setted;
	DCB dcb;
	bool commtimeouts_setted;
	COMMTIMEOUTS commtimeouts;
	bool read_requested;
	bool write_requested;
	DWORD write_left;
	enum {
		STATE_CLOSE,
		STATE_OPEN,
		STATE_ERROR,
	} state;
	bool check_line_state_before_send;
} comdata_t;

static void *init(void)
{
	comdata_t *p = (comdata_t *)calloc(sizeof(*p), 1);
	if (p == NULL) {
		// no memory
		return NULL;
	}
	p->dcb_setted = false;
	p->commtimeouts_setted = false;
	p->state = comdata_t::STATE_CLOSE;
	return p;
}

static DWORD destroy(device_t *device)
{
	comdata_t *p = (comdata_t *)device->private_data;
	if (p->port_name != NULL) {
		free(p->port_name);
	}
	free(p);

	free(device);
	return ERROR_SUCCESS;
}

#define CommInQueSize 8192
#define CommOutQueSize 2048

/**
 *	�N���[�Y
 */
static DWORD close(device_t *device)
{
	comdata_t *p = (comdata_t *)device->private_data;
	if (p == NULL) {
		return ERROR_HANDLES_CLOSED;
	}

	if (p->state == comdata_t::STATE_CLOSE) {
		return ERROR_SUCCESS;
	}

	CloseHandle(p->h);
	p->h = NULL;
	CloseHandle(p->rol.hEvent);
	p->rol.hEvent = NULL;
	CloseHandle(p->wol.hEvent);
	p->wol.hEvent = NULL;

	p->state = comdata_t::STATE_CLOSE;

	return ERROR_SUCCESS;
}

static DWORD open(device_t *device)
{
	comdata_t *p = (comdata_t *)device->private_data;
	HANDLE h;
	h = CreateFileW(p->port_name, GENERIC_READ | GENERIC_WRITE, 0, NULL,
					OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		DWORD e = GetLastError();
		return e;
	}

	BOOL r;
	r = GetCommProperties(h, &p->device_prop);
	assert(r == TRUE);
	// p->device_prop.dwMaxTxQueue == 0 �̂Ƃ��́A�ő�l�Ȃ�(�h���C�o�����܂�����Ă����炵��)

	DWORD DErr;
	ClearCommError(h, &DErr, NULL);
	SetupComm(h, CommInQueSize, CommOutQueSize);
	PurgeComm(h, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);

	if (p->commtimeouts_setted) {
		r = SetCommTimeouts(h, &p->commtimeouts);
		assert(r == TRUE);
	}

	r = GetCommTimeouts(h, &p->commtimeouts);
	assert(r == TRUE);

	if (p->dcb_setted) {
		if (p->dcb.XonChar == p->dcb.XoffChar) {
			p->dcb.XonChar = 0x11;
			p->dcb.XoffChar = 0x13;
		}
		r = SetCommState(h, &p->dcb);
		if (r == FALSE) {
			close(device);
			DWORD e = GetLastError();
			return e;
		}
	}

	r = GetCommState(h, &p->dcb);
	assert(r == TRUE);

	SetCommMask(h, 0);
	SetCommMask(h, EV_RXCHAR);

	memset(&p->rol, 0, sizeof(p->rol));
	p->rol.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	assert(p->rol.hEvent != NULL);
	memset(&p->wol, 0, sizeof(p->wol));
	p->wol.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	assert(p->wol.hEvent != NULL);
	p->h = h;

	p->state = comdata_t::STATE_OPEN;
	p->read_requested = false;
	p->write_requested = false;

	return ERROR_SUCCESS;
}

/**
 *	�y���f�B���O��Ԃ��`�F�b�N����
 *
 *	@param	readed				�ǂݍ��񂾃o�C�g��
 *								0	�ǂݍ��܂�Ă��Ȃ�
 *	@return	ERROR_SUCCESS		�ǂݍ��� (�y���f�B���O��ԏI��)
 *	@return	ERROR_IO_PENDING	�ǂݍ��ݑ҂�(����)
 *	@return	etc					�G���[
 */
static DWORD wait_read(device_t *device, size_t *readed)
{
	comdata_t *p = (comdata_t *)device->private_data;
	HANDLE h = p->h;

	if (p->state != comdata_t::STATE_OPEN) {
		return ERROR_NOT_READY;
	}
	if (p->read_requested == false) {
		// ���N�G�X�g���Ă��Ȃ��̂ɑ҂���ԂɂȂ���
		return ERROR_INVALID_OPERATION;
	}

	DWORD dwMilliseconds = 0;
	DWORD wait = WaitForSingleObject(p->rol.hEvent, dwMilliseconds);
	if (wait == WAIT_TIMEOUT) {
		// �܂���M���Ă��Ȃ�
		*readed = 0;
		return ERROR_IO_PENDING;
	}
	else if (wait == WAIT_OBJECT_0) {
		// �C�x���g����(��M����/�G���[�c)
		DWORD readed_;
		BOOL b = GetOverlappedResult(h, &p->rol, &readed_, TRUE);
		if (b) {
			// �ǂݍ��߂�
			*readed = readed_;
			p->read_requested = false;
			return ERROR_SUCCESS;
		}
		else {
			DWORD e = GetLastError();
			p->state = comdata_t::STATE_ERROR;
			*readed = 0;
			return e;
		}
	}
	else if (wait == WAIT_ABANDONED) {
		// �ǂ�ȂƂ���������? event�ُ�?
		p->state = comdata_t::STATE_ERROR;
		*readed = 0;
		return ERROR_INVALID_OPERATION;
	}
	else {
		// WAIT_FAILED
		p->state = comdata_t::STATE_ERROR;
		*readed = 0;
		DWORD e = GetLastError();
		return e;
	}
#if 0
	BOOL r = WaitCommEvent(h,&Evt, p->rol);
	if (r == FALSE) {
		// error
		DWORD DErr = GetLastError();
		if (DErr == ERROR_OPERATION_ABORTED) {
			// USB com port is removed
			printf("unpluged\n");
			return ERROR_OPERATION_ABORTED;
		}
		// �N���A���Ă�����?
		ClearCommError(h,&DErr,NULL);
	}
#endif
}

/**
 *	�f�[�^�ǂݍ��݁A�u���b�N���Ȃ�
 *
 *	@param	buf					�ǂݍ��ރA�h���X
 *								��œǂ܂�邩������Ȃ��̂Œ���
 *	@param	readed				�ǂݍ��񂾃o�C�g��
 *	@return	ERROR_SUCCESS		�ǂݍ���
 *								(readed = 0 �̏ꍇ���l�����邱��)
 *	@return	ERROR_IO_PENDING	�ǂݍ��ݑ҂�
 *								wait_read() �Ŋ�����҂�
 */
static DWORD read(device_t *device, uint8_t *buf, size_t buf_len, size_t *readed)
{
	comdata_t *p = (comdata_t *)device->private_data;
	HANDLE h = p->h;
	DWORD err = ERROR_SUCCESS;

	if (p->state != comdata_t::STATE_OPEN) {
		return ERROR_NOT_READY;
	}
	if (p->read_requested) {
		// �G���[�A���N�G�X�g��
		return ERROR_INVALID_OPERATION;
	}

	DWORD readed_;
	BOOL r = ReadFile(h, buf, (DWORD)buf_len, &readed_, &p->rol);
	if (!r) {
		*readed = 0;
		err = GetLastError();
		if (err == ERROR_IO_PENDING) {
			p->read_requested = true;
			return ERROR_IO_PENDING;
		}
		else {
			// �����G���[
			p->state = comdata_t::STATE_ERROR;
			p->read_requested = false;
			readed_ = 0;
			return err;
		}
	}

	*readed = readed_;
	return ERROR_SUCCESS;
}

/**
 *	�y���f�B���O��Ԃ��`�F�b�N����
 *
 *	@param		writed				�������܂ꂽ�o�C�g��
 *									0	�ǂݍ��܂�Ă��Ȃ�
 *	@return	ERROR_SUCCESS		�ǂݍ��� (�y���f�B���O��ԏI��)
 *	@return	ERROR_IO_PENDING	�ǂݍ��ݑ҂�(����)
 *	@return	etc					�G���[
 */
static DWORD wait_write(device_t *device, size_t *writed)
{
	comdata_t *p = (comdata_t *)device->private_data;
	HANDLE h = p->h;

	if (p->state != comdata_t::STATE_OPEN) {
		return ERROR_NOT_READY;
	}

	if (p->write_requested == false) {
		// ���N�G�X�g���Ă��Ȃ��̂ɑ҂���ԂɂȂ���
		return ERROR_INVALID_OPERATION;
	}

	//const DWORD timeout_ms = INFINITE;
	const DWORD timeout_ms = 0;
	DWORD wait = WaitForSingleObject(p->wol.hEvent, timeout_ms);
	if (wait == WAIT_TIMEOUT) {
		// �܂����M���Ă��Ȃ�
		*writed = 0;
		return ERROR_IO_PENDING;
	}
	else if (wait == WAIT_OBJECT_0) {
		// �C�x���g����(��������/�G���[�c)
		DWORD writed_;
		DWORD r = GetOverlappedResult(h, &p->wol, &writed_, FALSE);
		if (r) {
			*writed = writed_;
			p->write_left -= writed_;
			if (p->write_left == 0) {
				// ���M����
				p->write_requested = false;
				return ERROR_SUCCESS;
			}
			else {
				// �܂��y���f�B���O��
				return ERROR_IO_PENDING;
			}
		}
		else {
			DWORD e = GetLastError();
			p->state = comdata_t::STATE_ERROR;
			*writed = 0;
			return e;
		}
	}
	else {
		// WAIT_FAILED
		p->state = comdata_t::STATE_ERROR;
		*writed = 0;
		DWORD e = GetLastError();
		return e;
	}
}

/**
 *	��������
 *
 *	@return	ERROR_SUCCESS		�������݊���
 *	@return	ERROR_IO_PENDING	�������ݒ�
 *								wait_write() �Ŋ�����҂�
 *	@return	etc					�G���[
 */
static DWORD write(device_t *device, const void *buf, size_t buf_len, size_t *writed)
{
	comdata_t *p = (comdata_t *)device->private_data;
	HANDLE h = p->h;
	DWORD err = ERROR_SUCCESS;

	if (p->state != comdata_t::STATE_OPEN) {
		return ERROR_NOT_READY;
	}

	if (buf_len == 0) {
		return ERROR_SUCCESS;
	}

	if (p->write_requested == true) {
		// �G���[�A���N�G�X�g��
		return ERROR_INVALID_OPERATION;
	}

#if 0
	DWORD Errors;
	COMSTAT Comstat;
	ClearCommError(h, &Errors, &Comstat);
	if (Comstat.fCtsHold != 0) {
		return ERROR_SUCCESS;
	}
#endif

	if (p->check_line_state_before_send) {
		DWORD modem_state;
		GetCommModemStatus(h, &modem_state);
		if ((modem_state & MS_CTS_ON) == 0) {
			*writed = 0;
			return ERROR_SUCCESS;
		}
	}

#if 0
	// ���M�T�C�Y�𒲐�
	if (buf_len > Comstat.cbOutQue) {
		buf_len = Comstat.cbOutQue;
	}
#endif

	if (writed != NULL) {
		*writed = 0;
	}

	p->write_left = (DWORD)buf_len;
	DWORD writed_;
	BOOL r = WriteFile(h, buf, (DWORD)buf_len, &writed_, &p->wol);
	if (!r) {
		err = GetLastError();
		if (err == ERROR_IO_PENDING) {
			//const DWORD timeout_ms = INFINITE;
			const DWORD timeout_ms = 0;
			DWORD wait = WaitForSingleObject(p->wol.hEvent, timeout_ms);
			if (wait == WAIT_TIMEOUT) {
				// �܂����M���Ă��Ȃ�
				p->write_requested = true;
				return ERROR_IO_PENDING;
			}
			else if (wait == WAIT_OBJECT_0) {
				// �C�x���g����(�������݊���/�G���[�c)
				r = GetOverlappedResult(h, &p->wol, &writed_, FALSE);
				if (r) {
					// �������݊���
					goto write_complete;
				}
				else {
					err = GetLastError();
					p->state = comdata_t::STATE_ERROR;
					return err;
				}
			}
			else {	// wait == WAIT_ABANDONED || WAIT_FAILED
				p->state = comdata_t::STATE_ERROR;
				DWORD e = GetLastError();
				return e;
			}
		}
		else {
			p->state = comdata_t::STATE_ERROR;
			DWORD e = GetLastError();
			return e;
		}
	}

	// �������݊���
write_complete:
	if (writed != NULL) {
		*writed = writed_;
	}
	p->write_left -= writed_;
	if (p->write_left != 0) {
		// ���ׂď������ݍς݂ł͂Ȃ�,pending
		p->write_requested = true;
		return ERROR_IO_PENDING;
	}
	return ERROR_SUCCESS;
}

static DWORD ctrl(device_t *device, device_ctrl_request request, ...)
{
	comdata_t *p = (comdata_t *)device->private_data;
	va_list ap;
	va_start(ap, request);
	DWORD retval = ERROR_CALL_NOT_IMPLEMENTED;

	switch (request) {
	case SET_PORT_NAME: {
		const wchar_t *s = (wchar_t *)va_arg(ap, wchar_t *);
		if (p->port_name != NULL) {
			free(p->port_name);
		}
		p->port_name = _wcsdup(s);
		retval = ERROR_SUCCESS;
		break;
	}
	case GET_RAW_HANDLE: {
		HANDLE *handle = va_arg(ap, HANDLE *);
		*handle = p->h;
		retval = ERROR_SUCCESS;
		break;
	}
	case SET_COM_DCB: {
		DCB *dcb = va_arg(ap, DCB *);
		p->dcb = *dcb;
		p->dcb_setted = true;
		retval = ERROR_SUCCESS;
		break;
	}
	case GET_COM_DCB: {
		DCB *dcb = va_arg(ap, DCB *);
		*dcb = p->dcb;
		retval = ERROR_SUCCESS;
		break;
	}
	case SET_COM_TIMEOUTS: {
		COMMTIMEOUTS *commtimeouts = va_arg(ap, COMMTIMEOUTS *);
		p->commtimeouts = *commtimeouts;
		p->commtimeouts_setted = true;
		retval = ERROR_SUCCESS;
		break;
	}
	case GET_COM_TIMEOUTS: {
		COMMTIMEOUTS *commtimeouts = va_arg(ap, COMMTIMEOUTS *);
		*commtimeouts = p->commtimeouts;
		retval = ERROR_SUCCESS;
		break;
	}
	case SET_CHECK_LINE_STATE_BEFORE_SEND: {
		int check_line_state = va_arg(ap, int);
		p->check_line_state_before_send = check_line_state;
		retval = ERROR_SUCCESS;
		break;
	}
	case GET_CHECK_LINE_STATE_BEFORE_SEND: {
		int *check_line_state = va_arg(ap, int *);
		*check_line_state = (int)p->check_line_state_before_send;
		retval = ERROR_SUCCESS;
		break;
	}
	case OPEN_CONFIG_DIALOG: {
		COMMCONFIG cc;
		DWORD size = sizeof(cc);
		BOOL r = GetCommConfig(p->h, &cc, &size);
		assert(r == TRUE);
		r = CommConfigDialogW(p->port_name, NULL, &cc);
		if (r == TRUE) {
			r = SetCommConfig(p->h, &cc, size);
			assert(r == TRUE);
			r = GetCommState(p->h, &p->dcb);
			assert(r == TRUE);
		}
		break;
	}
	}

	va_end(ap);
	return retval;
}

static const device_ope ope = {
	destroy,
	open,
	close,
	read,
	wait_read,
	write,
	wait_write,
	ctrl,
};

/**
 *	com�|�[�g�f�o�C�X�쐬
 */
DWORD com_init(device_t **device)
{
	device_t *dev = (device_t *)calloc(sizeof(*dev), 1);
	if (dev == NULL) {
		return ERROR_NOT_ENOUGH_MEMORY;
	}
	void *data = init();
	if (data == NULL) {
		free(dev);
		return ERROR_NOT_ENOUGH_MEMORY;
	}
	dev->ope = &ope;
	dev->private_data = data;
	*device = dev;
	return ERROR_SUCCESS;
}
