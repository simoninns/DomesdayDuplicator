/***********************************************************************************************************************\
 * Program Name		:	main.cpp										*
 * Author		:	V. Radhakrishnan ( rk@atr-labs.com )							*
 * License		:	GPL Ver 2.0										*
 * Copyright		:	Cypress Semiconductors Inc. / ATR-LABS							*
 * Date written		:	July 7, 2012										*
 * Modification Notes	:												*
 * 															*
 * This program is the main GUI program for cyusb_suite for linux							*
\***********************************************************************************************************************/
#include <QtCore>
#include <QtGui>
#include <QtNetwork>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>

#include "../include/controlcenter.h"
#include "../include/cyusb.h"

ControlCenter *mainwin = NULL;
QProgressBar  *mbar = NULL;
QStatusBar *sb = NULL;
cyusb_handle  *h = NULL;
int num_devices_detected;
int current_device_index = -1;

static QLocalServer server(0);
static QTime *isoc_time;


extern int sigusr1_fd[2];
extern char pidfile[256];
extern char logfile[256];
extern int logfd;
extern int pidfd;

extern int fx2_ram_download(const char *filename, int extension);
extern int fx2_eeprom_download(const char *filename, int large);
extern int fx3_usbboot_download(const char *filename);
extern int fx3_i2cboot_download(const char *filename);
extern int fx3_spiboot_download(const char *filename);

struct DEVICE_SUMMARY {
	int	ifnum;
	int	altnum;
	int	epnum;
	int	eptype;
	int	maxps;
	int	interval;
};

static struct DEVICE_SUMMARY summ[100];
static int summ_count = 0;
static unsigned int cum_data_in;
static unsigned int cum_data_out;
static int data_count;
static struct libusb_transfer *transfer = NULL;
static unsigned char *isoc_databuf = NULL;
static int totalout, totalin, pkts_success, pkts_failure;

static int fd_outfile, fd_infile;


static void libusb_error(int errno, const char *detailedText)
{
	char msg[30];
	char tbuf[60];

	memset(msg,'\0',30);
	memset(tbuf,'\0',60);
	QMessageBox mb;
	if ( errno == LIBUSB_ERROR_IO )
		strcpy(msg, "LIBUSB_ERROR_IO");
	else if ( errno == LIBUSB_ERROR_INVALID_PARAM )
		strcpy(msg, "LIBUSB_ERROR_INVALID_PARAM" );
	else if ( errno == LIBUSB_ERROR_ACCESS )
		strcpy(msg, "LIBUSB_ERROR_ACCESS");
	else if ( errno == LIBUSB_ERROR_NO_DEVICE )
		strcpy(msg, "LIBUSB_ERROR_NO_DEVICE");
	else if ( errno == LIBUSB_ERROR_NOT_FOUND )
		strcpy(msg, "LIBUSB_ERROR_NOT_FOUND");
	else if ( errno == LIBUSB_ERROR_BUSY )
		strcpy(msg, "LIBUSB_ERROR_BUSY");
	else if ( errno == LIBUSB_ERROR_TIMEOUT )
		strcpy(msg, "LIBUSB_ERROR_TIMEOUT");
	else if ( errno == LIBUSB_ERROR_OVERFLOW )
		strcpy(msg, "LIBUSB_ERROR_OVERFLOW");
	else if ( errno == LIBUSB_ERROR_PIPE )
		strcpy(msg, "LIBUSB_ERROR_PIPE");
	else if ( errno == LIBUSB_ERROR_INTERRUPTED )
		strcpy(msg, "LIBUSB_ERROR_INTERRUPTED");
	else if ( errno == LIBUSB_ERROR_NO_MEM )
		strcpy(msg, "LIBUSB_ERROR_NO_MEM");
	else if ( errno == LIBUSB_ERROR_NOT_SUPPORTED )
		strcpy(msg, "LIBUSB_ERROR_NOT_SUPPORTED");
	else if ( errno == LIBUSB_ERROR_OTHER )
		strcpy(msg, "LIBUSB_ERROR_OTHER");
	else strcpy(msg, "LIBUSB_ERROR_UNDOCUMENTED");

	sprintf(tbuf,"LIBUSB_ERROR NO : %d, %s",errno,msg);
	mb.setText(tbuf);
	mb.setDetailedText(detailedText);
	mb.exec();
	return;
}

static void update_devlist()
{
	int i, r, num_interfaces, index = 0;
	char tbuf[60];
	struct libusb_config_descriptor *config_desc = NULL;

	mainwin->listWidget->clear();

	for ( i = 0; i < num_devices_detected; ++i ) {
		h = cyusb_gethandle(i);
		sprintf(tbuf,"VID=%04x,PID=%04x,BusNum=%02x,Addr=%d",
				cyusb_getvendor(h), cyusb_getproduct(h),
				cyusb_get_busnumber(h), cyusb_get_devaddr(h));
		mainwin->listWidget->addItem(QString(tbuf));
		r = cyusb_get_active_config_descriptor (h, &config_desc);
		if ( r ) {
			libusb_error(r, "Error in 'get_active_config_descriptor' ");
			return;
		}
		num_interfaces = config_desc->bNumInterfaces;
		while (num_interfaces){
			if (cyusb_kernel_driver_active (h, index)){
				cyusb_detach_kernel_driver (h, index);
			}
			index++;
			num_interfaces--;
		}
		cyusb_free_config_descriptor (config_desc);
	}
}
static void disable_vendor_extensions()
{
	mainwin->rb3_ramdl->setEnabled(FALSE);
	mainwin->rb3_ramup->setEnabled(FALSE);
	mainwin->rb3_eedl->setEnabled(FALSE);
	mainwin->rb3_eeup->setEnabled(FALSE);
	mainwin->rb3_getchip->setEnabled(FALSE);
	mainwin->rb3_renum->setEnabled(FALSE);
}

static void enable_vendor_extensions()
{
	mainwin->rb3_ramdl->setEnabled(TRUE);
	mainwin->rb3_ramup->setEnabled(TRUE);
	mainwin->rb3_eedl->setEnabled(TRUE);
	mainwin->rb3_eeup->setEnabled(TRUE);
	mainwin->rb3_getchip->setEnabled(TRUE);
	mainwin->rb3_renum->setEnabled(TRUE);
	mainwin->rb3_custom->setChecked(TRUE);
}

static void detect_device(void)
{
	int r;
	unsigned char byte = 0;

	r = cyusb_control_transfer(h, 0xC0, 0xA0, 0xE600, 0x00, &byte, 1, 1000);
	if ( r == 1 ) {
		mainwin->label_devtype->setText("FX2");
		enable_vendor_extensions();
		mainwin->tab_4->setEnabled(TRUE);
		mainwin->tab_5->setEnabled(FALSE);
		mainwin->tab2->setCurrentIndex(0);
	}
	else {
		mainwin->label_devtype->setText("FX3");
		disable_vendor_extensions();
		mainwin->rb3_custom->setChecked(TRUE);
		mainwin->tab_4->setEnabled(FALSE);
		mainwin->tab_5->setEnabled(TRUE);
		mainwin->tab2->setCurrentIndex(1);
	}
}

void ControlCenter::on_pb4_selfile_clicked()
{
	QString filename;
	if ( (current_device_index == -1) || (mainwin->label_if->text() == "") || (mainwin->label_aif->text() == "") ) {
		QMessageBox mb;
		mb.setText("No device+iface+alt-iface has been selected yet !\n");
		mb.exec();
		return ;
	}
	filename = QFileDialog::getOpenFileName(this, "Select file to download...", ".", "Image files (*.img)");
	mainwin->label4_file->setText(filename);
	if ( filename != "" ) {
		mainwin->pb4_start->setEnabled(TRUE);
	}

}

void ControlCenter::on_pb4_start_clicked()
{
	int r = 0;

	if ( mainwin->rb4_ram->isChecked() ) 
		r = fx3_usbboot_download(qPrintable(mainwin->label4_file->text()));
	else if ( mainwin->rb4_i2c->isChecked() ) 
		r = fx3_i2cboot_download(qPrintable(mainwin->label4_file->text()));
	else if ( mainwin->rb4_spi->isChecked() ) 
		r = fx3_spiboot_download(qPrintable(mainwin->label4_file->text()));

	if ( r ) {
		QMessageBox mb;
		mb.setText("Error in download");
		mb.exec();
	}
	else {
		QMessageBox mb;
		mb.setText("Successfully downloaded");
		mb.exec();
	}
}

void ControlCenter::on_pb4_clear_clicked()
{
	mainwin->lw4_display->clear();
}

static void check_for_kernel_driver(void)
{
	int r;
	int v = mainwin->sb_selectIf->value();

	r = cyusb_kernel_driver_active(h, v);
	if ( r == 1 ) {
		mainwin->cb_kerneldriver->setEnabled(TRUE);
		mainwin->cb_kerneldriver->setChecked(TRUE);
		mainwin->pb_kerneldetach->setEnabled(TRUE);
		mainwin->cb_kerneldriver->setEnabled(FALSE);
	}
	else {
		mainwin->cb_kerneldriver->setEnabled(FALSE);
		mainwin->cb_kerneldriver->setChecked(FALSE);
		mainwin->pb_kerneldetach->setEnabled(FALSE);
	}
}

static void update_bulk_endpoints()
{
	bool ok;
	char tbuf[4];
	int i;

	int iface   = mainwin->label_if->text().toInt(&ok, 10);
	int aiface  = mainwin->label_aif->text().toInt(&ok, 10);

	mainwin->cb6_in->clear();
	mainwin->cb6_out->clear();

	for ( i = 0; i < summ_count; ++i ) {
		if ( summ[i].ifnum != iface ) continue;
		if ( summ[i].altnum != aiface ) continue;
		if ( !(summ[i].eptype & 0x02 ) ) continue;
		sprintf(tbuf,"%02x",summ[i].epnum);
		if ( summ[i].epnum & 0x80 )
			mainwin->cb6_in->addItem(tbuf);
		else
			mainwin->cb6_out->addItem(tbuf);
	}
}

static void update_isoc_endpoints()
{
	bool ok;
	char tbuf[6];
	int i;

	int iface   = mainwin->label_if->text().toInt(&ok, 10);
	int aiface  = mainwin->label_aif->text().toInt(&ok, 10);

	mainwin->cb7_in->clear();
	mainwin->cb7_out->clear();

	for ( i = 0; i < summ_count; ++i ) {
		if ( summ[i].ifnum != iface ) continue;
		if ( summ[i].altnum != aiface ) continue;
		if ( !(summ[i].eptype & 0x01 ) ) continue;
		sprintf(tbuf,"%02x",summ[i].epnum);
		if ( summ[i].epnum & 0x80 ) {
			mainwin->cb7_in->addItem(tbuf);
		}
		else {
			mainwin->cb7_out->addItem(tbuf);
		}
	}
}

void ControlCenter::on_pb_setAltIf_clicked()
{
	int r1, r2;
	char tval[3];
	int i = mainwin->sb_selectIf->value();
	int a = mainwin->sb_selectAIf->value();

	r1 = cyusb_claim_interface(h, i);
	if ( r1 == 0 ) {
		r2 = cyusb_set_interface_alt_setting(h, i, a);
		if ( r2 != 0 ) {
			libusb_error(r2, "Error in setting Alternate Interface");
		}
	}
	else {
		libusb_error(r1, "Error in claiming interface");
	}
	sprintf(tval,"%d",a);
	mainwin->label_aif->setText(tval);
	check_for_kernel_driver();
	update_bulk_endpoints();
	update_isoc_endpoints();
}

void ControlCenter::on_pb_setIFace_clicked()
{
	int r;
	char tval[3];
	int N, M;

	struct libusb_config_descriptor *config_desc = NULL;

	r = cyusb_get_active_config_descriptor(h, &config_desc);
	if ( r ) libusb_error(r, "Error in 'get_active_config_descriptor' ");

	N = config_desc->interface[mainwin->sb_selectIf->value()].num_altsetting;
	sprintf(tval,"%d",N);
	mainwin->le_numAlt->setText(tval);
	mainwin->sb_selectAIf->setMaximum(N - 1);
	mainwin->sb_selectAIf->setValue(0);
	mainwin->sb_selectAIf->setEnabled(TRUE);
	mainwin->pb_setAltIf->setEnabled(TRUE);
	M = mainwin->sb_selectIf->value();
	sprintf(tval,"%d",M);
	mainwin->label_if->setText(tval);
	mainwin->on_pb_setAltIf_clicked();
}

static void update_summary(void)
{
	char tbuf[100];
	int i;
	char ifnum[7];
	char altnum[7];
	char epnum[7];
	char iodirn[7];
	char iotype[7];
	char maxps[7];
	char interval[7];

	ifnum[6] = altnum[6] = epnum[6] = iodirn[6] = iotype[6] = maxps[6] = interval[6] = '\0';

	memset(tbuf,'\0',100);
	mainwin->lw_summ->clear();

	for ( i = 0; i < summ_count; ++i ) {
		sprintf(ifnum,"%2d    ",summ[i].ifnum);
		sprintf(altnum,"%2d    ",summ[i].altnum);
		sprintf(epnum,"%2x    ",summ[i].epnum);
		if ( summ[i].epnum & 0x80 ) 
			strcpy(iodirn,"IN ");
		else strcpy(iodirn,"OUT");
		if ( summ[i].eptype & 0x00 )
			strcpy(iotype,"CTRL");
		else if ( summ[i].eptype & 0x01 )
			strcpy(iotype,"Isoc");
		else if ( summ[i].eptype & 0x02 )
			strcpy(iotype,"Bulk");
		else strcpy(iotype,"Intr");
		sprintf(maxps,"%4d  ",summ[i].maxps);
		sprintf(interval,"%3d   ",summ[i].interval);
		if ( i == 0 ) {
			sprintf(tbuf,"%-6s %-6s %-6s %-6s %-6s %-6s %-6s",
					ifnum,altnum,epnum,iodirn,iotype,maxps,interval);
			mainwin->lw_summ->addItem(QString(tbuf));
		}
		else {
			if ( summ[i].ifnum == summ[i-i].ifnum ) {
				memset(ifnum,' ',6);
				if ( summ[i].altnum == summ[i-1].altnum ) {
					memset(altnum,' ',6);
					sprintf(tbuf,"%-6s %-6s %-6s %-6s %-6s %-6s %-6s",
							ifnum,altnum,epnum,iodirn,iotype,maxps,interval);
					mainwin->lw_summ->addItem(QString(tbuf));
				}
				else { sprintf(tbuf,"%-6s %-6s %-6s %-6s %-6s %-6s %-6s",
						ifnum,altnum,epnum,iodirn,iotype,maxps,interval);
				mainwin->lw_summ->addItem("");
				mainwin->lw_summ->addItem(tbuf);
				}
			}
			else { sprintf(tbuf,"%-6s %-6s %-6s %-6s %-6s %-6s %-6s",
					ifnum,altnum,epnum,iodirn,iotype,maxps,interval);
			mainwin->lw_summ->addItem("");
			mainwin->lw_summ->addItem(tbuf);
			}
		}
	}
}


void get_config_details()
{
	int r;
	int i, j, k;
	char tbuf[60];
	char tval[3];
	struct libusb_config_descriptor *desc = NULL;

	h = cyusb_gethandle(current_device_index);

	r = cyusb_get_active_config_descriptor(h, &desc);
	if ( r ) {
		libusb_error(r, "Error getting configuration descriptor");
		return ;
	}
	sprintf(tval,"%d",desc->bNumInterfaces);
	mainwin->le_numIfaces->setReadOnly(TRUE);
	mainwin->le_numIfaces->setText(tval);
	mainwin->sb_selectIf->setMaximum(desc->bNumInterfaces - 1);

	sprintf(tbuf,"<CONFIGURATION>");
	mainwin->lw_desc->addItem(QString(tbuf));
	sprintf(tbuf,"bLength             = %d",   desc->bLength);
	mainwin->lw_desc->addItem(QString(tbuf));
	sprintf(tbuf,"bDescriptorType     = %d",   desc->bDescriptorType);
	mainwin->lw_desc->addItem(QString(tbuf));
	sprintf(tbuf,"TotalLength         = %d",   desc->wTotalLength);
	mainwin->lw_desc->addItem(QString(tbuf));
	sprintf(tbuf,"Num. of interfaces  = %d",   desc->bNumInterfaces);
	mainwin->lw_desc->addItem(QString(tbuf));
	sprintf(tbuf,"bConfigurationValue = %d",   desc->bConfigurationValue);
	mainwin->lw_desc->addItem(QString(tbuf));
	sprintf(tbuf,"iConfiguration      = %d",    desc->iConfiguration);
	mainwin->lw_desc->addItem(QString(tbuf));
	sprintf(tbuf,"bmAttributes        = %d",    desc->bmAttributes);
	mainwin->lw_desc->addItem(QString(tbuf));
	sprintf(tbuf,"Max Power           = %04d",  desc->MaxPower);
	mainwin->lw_desc->addItem(QString(tbuf));

	summ_count = 0;

	for ( i = 0; i < desc->bNumInterfaces; ++i ) {
		const struct libusb_interface *iface = desc->interface;
		mainwin->cb6_in->clear();
		mainwin->cb6_out->clear();
		for ( j = 0; j < iface[i].num_altsetting; ++j ) {
			sprintf(tbuf,"\t<INTERFACE>");
			mainwin->lw_desc->addItem(QString(tbuf));
			const struct libusb_interface_descriptor *ifd = iface[i].altsetting;
			sprintf(tbuf,"\tbLength             = %d",   ifd[j].bLength);
			mainwin->lw_desc->addItem(QString(tbuf));
			sprintf(tbuf,"\tbDescriptorType     = %d",   ifd[j].bDescriptorType);
			mainwin->lw_desc->addItem(QString(tbuf));
			sprintf(tbuf,"\tbInterfaceNumber    = %d",   ifd[j].bInterfaceNumber);
			mainwin->lw_desc->addItem(QString(tbuf));
			sprintf(tbuf,"\tbAlternateSetting   = %d",   ifd[j].bAlternateSetting);
			mainwin->lw_desc->addItem(QString(tbuf));
			sprintf(tbuf,"\tbNumEndpoints       = %d",   ifd[j].bNumEndpoints);
			mainwin->lw_desc->addItem(QString(tbuf));
			sprintf(tbuf,"\tbInterfaceClass     = %02x", ifd[j].bInterfaceClass);
			mainwin->lw_desc->addItem(QString(tbuf));
			sprintf(tbuf,"\tbInterfaceSubClass  = %02x", ifd[j].bInterfaceSubClass);
			mainwin->lw_desc->addItem(QString(tbuf));
			sprintf(tbuf,"\tbInterfaceProtcol   = %02x", ifd[j].bInterfaceProtocol);
			mainwin->lw_desc->addItem(QString(tbuf));
			sprintf(tbuf,"\tiInterface          = %0d",  ifd[j].iInterface);
			mainwin->lw_desc->addItem(QString(tbuf));


			for ( k = 0; k < ifd[j].bNumEndpoints; ++k ) {
				sprintf(tbuf,"\t\t<ENDPOINT>");
				mainwin->lw_desc->addItem(QString(tbuf));
				const struct libusb_endpoint_descriptor *ep = ifd[j].endpoint;
				sprintf(tbuf,"\t\tbLength             = %0d",  ep[k].bLength);
				mainwin->lw_desc->addItem(QString(tbuf));
				sprintf(tbuf,"\t\tbDescriptorType     = %0d",  ep[k].bDescriptorType);
				mainwin->lw_desc->addItem(QString(tbuf));
				sprintf(tbuf,"\t\tbEndpointAddress    = %02x", ep[k].bEndpointAddress);
				mainwin->lw_desc->addItem(QString(tbuf));
				sprintf(tbuf,"\t\tbmAttributes        = %d",   ep[k].bmAttributes);
				mainwin->lw_desc->addItem(QString(tbuf));

				summ[summ_count].ifnum    = ifd[j].bInterfaceNumber;
				summ[summ_count].altnum   = ifd[j].bAlternateSetting;
				summ[summ_count].epnum    = ep[k].bEndpointAddress;
				summ[summ_count].eptype   = ep[k].bmAttributes;
				summ[summ_count].maxps    = ep[k].wMaxPacketSize & 0x7ff;  /* ignoring bits 11,12 */
				summ[summ_count].interval = ep[k].bInterval;
				++summ_count;

				sprintf(tbuf,"\t\twMaxPacketSize      = %04x", (ep[k].wMaxPacketSize));
				mainwin->lw_desc->addItem(QString(tbuf));
				sprintf(tbuf,"\t\tbInterval           = %d",   ep[k].bInterval);
				mainwin->lw_desc->addItem(QString(tbuf));
				sprintf(tbuf,"\t\tbRefresh            = %d",   ep[k].bRefresh);
				mainwin->lw_desc->addItem(QString(tbuf));
				sprintf(tbuf,"\t\tbSynchAddress       = %d",   ep[k].bSynchAddress);
				mainwin->lw_desc->addItem(QString(tbuf));
				sprintf(tbuf,"\t\t</ENDPOINT>");
				mainwin->lw_desc->addItem(QString(tbuf));
			}
			sprintf(tbuf,"\t</INTERFACE>");
			mainwin->lw_desc->addItem(QString(tbuf));
		}

	}
	sprintf(tbuf,"</CONFIGURATION>");
	mainwin->lw_desc->addItem(QString(tbuf));

	cyusb_free_config_descriptor(desc);

	check_for_kernel_driver();
	update_summary();
	mainwin->on_pb_setIFace_clicked();
}

void get_device_details()
{
	int r;
	char tbuf[60];
	char tval[3];
	struct libusb_device_descriptor desc;
	struct libusb_config_descriptor *config_desc = NULL;

	h = cyusb_gethandle(current_device_index);
	if ( !h ) {
		printf("Error in getting a handle. curent_device_index = %d\n", current_device_index);
	}

	r = cyusb_get_device_descriptor(h, &desc);
	if ( r ) {
		libusb_error(r, "Error getting device descriptor");
		return ;
	}
	r = cyusb_get_active_config_descriptor(h, &config_desc);
	sprintf(tval,"%d",config_desc->bNumInterfaces);
	mainwin->le_numIfaces->setText(tval);
	mainwin->sb_selectIf->setEnabled(TRUE);
	mainwin->sb_selectIf->setMaximum(config_desc->bNumInterfaces - 1);
	mainwin->sb_selectIf->setValue(1);
	mainwin->pb_setIFace->setEnabled(TRUE);

	mainwin->lw_desc->clear();

	sprintf(tbuf,"<DEVICE>               ");
	mainwin->lw_desc->addItem(QString(tbuf));

	sprintf(tbuf,"bLength             = %d",   desc.bLength);
	mainwin->lw_desc->addItem(QString(tbuf));
	sprintf(tbuf,"bDescriptorType     = %d",   desc.bDescriptorType);
	mainwin->lw_desc->addItem(QString(tbuf));
	sprintf(tbuf,"bcdUSB              = %d",   desc.bcdUSB);
	mainwin->lw_desc->addItem(QString(tbuf));
	sprintf(tbuf,"bDeviceClass        = %d",   desc.bDeviceClass);
	mainwin->lw_desc->addItem(QString(tbuf));
	sprintf(tbuf,"bDeviceSubClass     = %d",   desc.bDeviceSubClass);
	mainwin->lw_desc->addItem(QString(tbuf));
	sprintf(tbuf,"bDeviceProtocol     = %d",   desc.bDeviceProtocol);
	mainwin->lw_desc->addItem(QString(tbuf));
	sprintf(tbuf,"bMaxPacketSize      = %d",   desc.bMaxPacketSize0);
	mainwin->lw_desc->addItem(QString(tbuf));
	sprintf(tbuf,"idVendor            = %04x", desc.idVendor);
	mainwin->lw_desc->addItem(QString(tbuf));
	sprintf(tbuf,"idProduct           = %04x", desc.idProduct);
	mainwin->lw_desc->addItem(QString(tbuf));
	sprintf(tbuf,"bcdDevice           = %d",   desc.bcdDevice);
	mainwin->lw_desc->addItem(QString(tbuf));
	sprintf(tbuf,"iManufacturer       = %d",   desc.iManufacturer);
	mainwin->lw_desc->addItem(QString(tbuf));
	sprintf(tbuf,"iProduct            = %d",   desc.iProduct);
	mainwin->lw_desc->addItem(QString(tbuf));
	sprintf(tbuf,"iSerialNumber       = %d",   desc.iSerialNumber);
	mainwin->lw_desc->addItem(QString(tbuf));
	sprintf(tbuf,"bNumConfigurations  = %d",   desc.bNumConfigurations);
	mainwin->lw_desc->addItem(QString(tbuf));
	sprintf(tbuf,"</DEVICE>               ");
	mainwin->lw_desc->addItem(QString(tbuf));


	check_for_kernel_driver();
	detect_device();
	mainwin->on_pb_setIFace_clicked();
	cyusb_free_config_descriptor (config_desc);
}

static void clear_widgets()
{
	mainwin->lw_desc->clear();
	mainwin->label_if->clear();
	mainwin->label_aif->clear();
	mainwin->le_numIfaces->clear();
	mainwin->le_numAlt->clear();
	mainwin->sb_selectIf->clear();
	mainwin->sb_selectIf->setEnabled(FALSE);
	mainwin->sb_selectAIf->setValue(0);
	mainwin->sb_selectAIf->setEnabled(FALSE);
	mainwin->lw_desc->clear();
	mainwin->cb_kerneldriver->setChecked(FALSE);
	mainwin->cb_kerneldriver->setEnabled(FALSE);
	mainwin->pb_setIFace->setEnabled(FALSE);
	mainwin->pb_kerneldetach->setEnabled(FALSE);
	mainwin->pb_setAltIf->setEnabled(FALSE);
	mainwin->label_devtype->setText("");
}

static void set_if_aif()
{
	int r1, r2;
	char tval[5];

	int i = mainwin->sb_selectIf->value();
	int a = mainwin->sb_selectAIf->value();

	r1 = cyusb_claim_interface(h, i);
	if ( r1 == 0 ) {
		r2 = cyusb_set_interface_alt_setting(h, i, a);
		if ( r2 != 0 ) {
			libusb_error(r2, "Error in setting Alternate Interface");
			return;
		}
	}
	else {
		libusb_error(r1, "Error in setting Interface");
		return;
	}
	sprintf(tval,"%d",a);
	mainwin->label_aif->setText(tval);
}

void ControlCenter::on_listWidget_itemClicked(QListWidgetItem *item)
{
	item = item;
	clear_widgets();
	current_device_index = mainwin->listWidget->currentRow();
	get_device_details();
	get_config_details();
	set_if_aif();
}

static void set_tool_tips(void)
{
	mainwin->cb_kerneldriver->setToolTip("If checked, implies interface has been claimed");
	mainwin->pb_kerneldetach->setToolTip("Releases a claimed interface");
	mainwin->le3_wval->setToolTip("Enter address for A2/A3, perhaps Custom commands");
}

void ControlCenter::on_pb_kerneldetach_clicked()
{
	int r;

	r = cyusb_detach_kernel_driver(h, mainwin->sb_selectIf->value());
	if ( r == 0 ) {
		mainwin->cb_kerneldriver->setEnabled(TRUE);
		mainwin->cb_kerneldriver->setChecked(FALSE);
		mainwin->cb_kerneldriver->setEnabled(FALSE);
		mainwin->pb_kerneldetach->setEnabled(FALSE);
		mainwin->label_aif->clear();
	}
	else {
		libusb_error(r, "Error in detaching kernel mode driver!");
		return;
	}
}

void ControlCenter::on_rb1_ram_clicked()
{
	mainwin->groupBox_3->setVisible(TRUE);
	mainwin->groupBox_3->setEnabled(TRUE);
	mainwin->rb_internal->setChecked(TRUE);
}

void ControlCenter::on_rb1_small_clicked()
{
	mainwin->groupBox_3->setVisible(FALSE);
}

void ControlCenter::on_rb1_large_clicked()
{
	mainwin->groupBox_3->setVisible(FALSE);
}

void ControlCenter::sigusr1_handler()
{
	int nbr;
	char tmp;

	mainwin->sn_sigusr1->setEnabled(false);
	nbr = read(sigusr1_fd[1], &tmp, 1);

	update_devlist();
	mainwin->sn_sigusr1->setEnabled(true);	
}

static void setup_handler(int signo)
{
	int nbw;
	char a = 1;
	int N;

	printf("Signal %d (=SIGUSR1) received !\n",signo);
	cyusb_close();
	N = cyusb_open();
	if ( N < 0 ) {
		printf("Error in opening library\n");
		exit(-1);
	}
	else if ( N == 0 ) {
		printf("No device of interest found\n");
		num_devices_detected = 0;
		current_device_index = -1;
	}
	else {
		printf("No of devices of interest found = %d\n",N);
		num_devices_detected = N;
		current_device_index = 0;
	}
	nbw = write(sigusr1_fd[0], &a, 1);
}

void ControlCenter::on_pb1_selfile_clicked()
{
	QString filename;
	if ( (current_device_index == -1) || (mainwin->label_if->text() == "") || (mainwin->label_aif->text() == "") ) {
		QMessageBox mb;
		mb.setText("No device+iface+alt-iface has been selected yet !\n");
		mb.exec();
		return ;
	}
	if ( mainwin->rb1_ram->isChecked() )
		filename = QFileDialog::getOpenFileName(this, "Select file to download...", ".", "Hex file (*.hex);;IIC file (*.iic);;BIN file (*.bin)");
	else
	       	filename = QFileDialog::getOpenFileName(this, "Select file to download...", ".", "IIC files (*.iic)");
	mainwin->label1_selfile->setText(filename);
	if ( filename != "" ) {
		mainwin->pb1_start->setEnabled(TRUE);
	}
}	

void ControlCenter::on_pb1_start_clicked()
{
	int r;

	if ( mainwin->rb1_ram->isChecked() ) {
		if ( mainwin->rb_internal->isChecked() )
			r = fx2_ram_download(qPrintable(mainwin->label1_selfile->text()), 0);
		else 
			r = fx2_ram_download(qPrintable(mainwin->label1_selfile->text()), 1);
	}
	else {
		if ( mainwin->rb1_large->isChecked() )
			r = fx2_eeprom_download(qPrintable(mainwin->label1_selfile->text()), 1);
		else
			r = fx2_eeprom_download(qPrintable(mainwin->label1_selfile->text()), 0);
	}

	if ( r ) {
		QMessageBox mb;
		mb.setText("Error in download");
		mb.exec();
	}
	else {
		QMessageBox mb;
		mb.setText("Successfully downloaded");
		mb.exec();
	}
}

void ControlCenter::on_pb_reset_clicked()
{
	int r;

	if ( current_device_index == -1 ) {
		QMessageBox mb;
		mb.setText("No device has been selected yet !\n");
		mb.exec();
		return ;
	}
	r = cyusb_reset_device(h);	
	QMessageBox mb;
	mb.setText("Device reset");
	mb.exec();
}

void ControlCenter::on_rb3_ramdl_clicked()
{
	mainwin->rb3_out->setChecked(TRUE);
	mainwin->gb_dir->setEnabled(FALSE);
	mainwin->le3_bm->setText("40");
	mainwin->le3_bm->setReadOnly(TRUE);
	mainwin->le3_br->setText("A3");
	mainwin->le3_br->setReadOnly(TRUE);
	mainwin->le3_wlen->setReadOnly(TRUE);
	mainwin->le3_out_hex->setEnabled(TRUE);
	mainwin->le3_out_ascii->setEnabled(TRUE);
	mainwin->le3_wind->setText("0000");
	mainwin->le3_wind->setReadOnly(TRUE);
	mainwin->le3_wlen->setText("");
}

void ControlCenter::on_rb3_ramup_clicked()
{
	mainwin->rb3_in->setChecked(TRUE);
	mainwin->gb_dir->setEnabled(FALSE);
	mainwin->le3_bm->setText("C0");
	mainwin->le3_bm->setReadOnly(TRUE);
	mainwin->le3_br->setText("A3");
	mainwin->le3_br->setReadOnly(TRUE);
	mainwin->le3_wlen->setReadOnly(FALSE);
	mainwin->le3_out_hex->setEnabled(FALSE);
	mainwin->le3_out_ascii->setEnabled(FALSE);
	mainwin->le3_wind->setText("0000");
	mainwin->le3_wind->setReadOnly(TRUE);

}

void ControlCenter::on_rb3_eedl_clicked()
{
	mainwin->rb3_out->setChecked(TRUE);
	mainwin->gb_dir->setEnabled(FALSE);
	mainwin->le3_bm->setText("40");
	mainwin->le3_bm->setReadOnly(TRUE);
	mainwin->le3_br->setText("A2");
	mainwin->le3_br->setReadOnly(TRUE);
	mainwin->le3_wlen->setReadOnly(TRUE);
	mainwin->le3_out_hex->setEnabled(TRUE);
	mainwin->le3_out_ascii->setEnabled(TRUE);
	mainwin->le3_wind->setText("0000");
	mainwin->le3_wind->setReadOnly(TRUE);
	mainwin->le3_wlen->setText("");
}


void ControlCenter::on_rb3_eeup_clicked()
{
	mainwin->rb3_in->setChecked(TRUE);
	mainwin->gb_dir->setEnabled(FALSE);
	mainwin->le3_bm->setText("C0");
	mainwin->le3_bm->setReadOnly(TRUE);
	mainwin->le3_br->setText("A2");
	mainwin->le3_br->setReadOnly(TRUE);
	mainwin->le3_wlen->setReadOnly(FALSE);
	mainwin->le3_out_hex->setEnabled(FALSE);
	mainwin->le3_out_ascii->setEnabled(FALSE);
	mainwin->le3_wind->setText("0000");
	mainwin->le3_wind->setReadOnly(TRUE);
}

void ControlCenter::on_rb3_getchip_clicked()
{
	mainwin->gb_dir->setEnabled(FALSE);
	mainwin->le3_bm->setText("C0");
	mainwin->le3_bm->setReadOnly(TRUE);
	mainwin->le3_br->setText("A6");
	mainwin->le3_br->setReadOnly(TRUE);
	mainwin->le3_out_hex->setEnabled(FALSE);
	mainwin->le3_out_ascii->setEnabled(FALSE);
	mainwin->le3_wind->setText("0000");
	mainwin->le3_wind->setReadOnly(TRUE);
	mainwin->le3_wlen->setText("01");
	mainwin->le3_wlen->setReadOnly(TRUE);
}

void ControlCenter::on_rb3_renum_clicked()
{
	mainwin->gb_dir->setEnabled(FALSE);
	mainwin->le3_bm->setText("40");
	mainwin->le3_bm->setReadOnly(TRUE);
	mainwin->le3_br->setText("A8");
	mainwin->le3_br->setReadOnly(TRUE);
	mainwin->le3_out_hex->setEnabled(FALSE);
	mainwin->le3_out_ascii->setEnabled(FALSE);
	mainwin->le3_wind->setText("0000");
	mainwin->le3_wind->setReadOnly(TRUE);
	mainwin->le3_wlen->setText("01");
	mainwin->le3_wlen->setReadOnly(TRUE);
}

void ControlCenter::on_rb3_custom_clicked()
{
	mainwin->rb3_out->setChecked(TRUE);
	mainwin->gb_dir->setEnabled(TRUE);
	mainwin->le3_bm->setReadOnly(FALSE);
	mainwin->le3_bm->setText("");
	mainwin->le3_br->setText("");
	mainwin->le3_br->setReadOnly(FALSE);
	mainwin->le3_wlen->setReadOnly(FALSE);
	mainwin->le3_wind->setText("");
	mainwin->le3_wind->setReadOnly(FALSE);
	mainwin->le3_out_hex->setEnabled(TRUE);
	mainwin->le3_out_ascii->setEnabled(TRUE);
	mainwin->le3_wlen->setText("");
}


void ControlCenter::on_rb3_out_clicked()
{
	if ( !mainwin->rb3_custom->isChecked() )
		mainwin->le3_bm->setText("40");
	mainwin->le3_out_hex->setEnabled(TRUE);
	mainwin->le3_out_ascii->setEnabled(TRUE);
	mainwin->le3_wlen->setReadOnly(TRUE);
}

void ControlCenter::on_rb3_in_clicked()
{
	if ( !mainwin->rb3_custom->isChecked() )
		mainwin->le3_bm->setText("C0");
	mainwin->le3_out_hex->setEnabled(FALSE);
	mainwin->le3_out_ascii->setEnabled(FALSE);
	mainwin->le3_wlen->setReadOnly(FALSE);
}

void ControlCenter::on_pb3_selfile_clicked()
{
	QString filename;
	if ( (current_device_index == -1) || (mainwin->label_if->text() == "") || (mainwin->label_aif->text() == "") ) {
		QMessageBox mb;
		mb.setText("No device+iface+alt-iface has been selected yet !\n");
		mb.exec();
		return ;
	}
	filename = QFileDialog::getOpenFileName(this, "Select file to download...", ".", "Image files (*.hex)");
	mainwin->lab3_selfile->setText(filename);
	if ( filename != "" ) {
		mainwin->pb3_dl->setEnabled(TRUE);
	}

}

void ControlCenter::on_le3_out_ascii_textChanged()
{
	char tbuf[3];
	QByteArray t = mainwin->le3_out_ascii->text().toAscii();
	sprintf(tbuf,"%02d",t.size());
	mainwin->le3_wlen->setText(tbuf);
}

void ControlCenter::on_le3_out_ascii_textEdited()
{
	char tbuf[3];
	QByteArray t = mainwin->le3_out_ascii->text().toAscii();
	sprintf(tbuf,"%02d",t.size());
	mainwin->le3_wlen->setText(tbuf);
	mainwin->le3_out_hex->setText(t.toHex());
}

void ControlCenter::on_le3_out_hex_textEdited()
{
	QRegExp rx("[0-9A-Fa-f]*");
	QValidator *validator = new QRegExpValidator(rx, this);
	mainwin->le3_out_hex->setValidator(validator);

	QByteArray t = mainwin->le3_out_hex->text().toAscii();
	QByteArray t2 = QByteArray::fromHex(t);
	mainwin->le3_out_ascii->setText(t2);
}

static void dump_data(unsigned char num_bytes, char *dbuf)
{
	int i, j, k, index, filler;
	char ttbuf[10];
	char finalbuf[256];
	char tbuf[256];
	int balance = 0;

	balance = num_bytes;
	index = 0;

	while ( balance > 0 ) {
		tbuf[0]  = '\0';
		if ( balance < 16 )
			j = balance;
		else j = 16;
		for ( i = 0; i < j; ++i ) {
			sprintf(ttbuf,"%02x ",(unsigned char)dbuf[index+i]);
			strcat(tbuf,ttbuf);
		}
		if ( balance < 16 ) {
			filler = 16-balance;
			for ( k = 0; k < filler; ++k ) 
				strcat(tbuf,"   ");
		}
		strcat(tbuf,": ");
		for ( i = 0; i < j; ++i ) {
			if ( !isprint(dbuf[index+i]) )
				strcat(tbuf,". ");
			else {
				sprintf(ttbuf,"%c ",dbuf[index+i]);
				strcat(tbuf,ttbuf);
			}
		}
		sprintf(finalbuf,"%s",tbuf);
		mainwin->lw3->addItem(finalbuf);
		balance -= j;
		index += j;
	}
}

void ControlCenter::on_pb_execvc_clicked()
{	
	int r;

	mainwin->lw3->clear();

	if ( ( mainwin->le3_bm->text() == "" )
			||   ( mainwin->le3_br->text() == "" )
			||   ( mainwin->le3_wval->text() == "" )
			||   (mainwin->le3_wind->text() == "" )
			||   (mainwin->le3_wlen->text() == "" ) ) {
		QMessageBox mb;
		mb.setText("Please fill up ALL fields\n");
		mb.exec();
		return ;
	}

	unsigned char bmRequest = strtoul(qPrintable(mainwin->le3_bm->text()), NULL, 16);
	unsigned char bRequest  = strtoul(qPrintable(mainwin->le3_br->text()), NULL, 16);
	unsigned short wValue   = strtoul(qPrintable(mainwin->le3_wval->text()), NULL, 16);
	unsigned short wIndex   = strtoul(qPrintable(mainwin->le3_wind->text()), NULL, 16);
	unsigned short wLength  = strtoul(qPrintable(mainwin->le3_wlen->text()),NULL, 10);
	char data[4096];


	printf("bmRequest=%02x, bRequest=%02x, wValue=%04x, wIndex=%04x, wLength=%d\n",
			bmRequest, bRequest, wValue, wIndex, wLength);

	if ( bmRequest == 0x40 ) {
		strncpy(data,qPrintable(mainwin->le3_out_ascii->text()),wLength);
		r = cyusb_control_transfer(h, bmRequest, bRequest, wValue, wIndex, (unsigned char *)data, wLength, 1000);
		if (r != wLength ) {
			QMessageBox mb;
			mb.setText("Error in executing vendor command!\n");
			mb.exec();
			return ;
		}
		else {
			QMessageBox mb;
			mb.setText("Successfully executed vendor command!\n");
			mb.exec();
			return ;
		}
	}
	else {
		memset(data,' ',4096);
		r = cyusb_control_transfer(h, bmRequest, bRequest, wValue, wIndex, (unsigned char *)data, wLength, 1000);
		if (r != wLength ) {
			QMessageBox mb;
			mb.setText("Error in executing vendor command!\n");
			mb.exec();
			return ;
		}
		else {
			QMessageBox mb;
			mb.setText("Successfully executed vendor command!\n");
			mb.exec();
			dump_data(r, data);
		}
	}
}

void ControlCenter::on_pb3_dl_clicked()
{
	fx2_ram_download(qPrintable(mainwin->lab3_selfile->text()), 0);
	QMessageBox mb;
	mb.setText("Completed download");
	mb.exec();
	return ;
}

void ControlCenter::on_le6_out_hex_textEdited()
{
	QRegExp rx("[0-9A-Fa-f]*");
	QValidator *validator = new QRegExpValidator(rx, this);
	mainwin->le6_out_hex->setValidator(validator);

	QByteArray t = mainwin->le6_out_hex->text().toAscii();
	QByteArray t2 = QByteArray::fromHex(t);
	mainwin->le6_out_ascii->setText(t2);
	mainwin->le6_outfile->setText("");
}

void ControlCenter::on_le6_out_ascii_textEdited()
{
	char tbuf[3];
	QByteArray t = mainwin->le6_out_ascii->text().toAscii();
	sprintf(tbuf,"%02d",t.size());
	mainwin->le6_size->setText(tbuf);
	mainwin->le6_out_hex->setText(t.toHex());
	mainwin->le6_outfile->setText("");
}

void ControlCenter::on_le6_out_ascii_textChanged()
{
	char tbuf[3];
	QByteArray t = mainwin->le6_out_ascii->text().toAscii();
	sprintf(tbuf,"%02d",t.size());
	mainwin->le6_size->setText(tbuf);
}

void ControlCenter::on_pb6_clear_clicked()
{
	mainwin->lw6->clear();
	mainwin->lw6_out->clear();
	mainwin->le6_out_hex->clear();
	mainwin->le6_out_ascii->clear();
	mainwin->label6_out->clear();
	mainwin->label6_in->clear();
	mainwin->le6_outfile->clear();
	mainwin->le6_infile->clear();
	mainwin->le6_size->clear();
	cum_data_in = cum_data_out = 0;
}

void ControlCenter::on_rb6_constant_clicked()
{
	le6_value->setReadOnly(FALSE);
	le6_size->setText("512");

}

void ControlCenter::on_rb6_random_clicked()
{
	le6_size->setText("512");
	le6_value->setReadOnly(TRUE);
}

void ControlCenter::on_rb6_inc_clicked()
{
	le6_size->setText("512");
	le6_value->setReadOnly(FALSE);
}


void ControlCenter::on_cb6_loop_clicked()
{
	if ( mainwin->cb6_loop->isChecked() ) {
		mainwin->pb6_rcv->setEnabled(FALSE);
	}
	else {
		mainwin->pb6_rcv->setEnabled(TRUE);
	}
}

static void dump_data6_out(int num_bytes, unsigned char *dbuf)
{
	int i, j, k, index, filler;
	char ttbuf[10];
	char finalbuf[256];
	char tbuf[256];
	int balance = 0;

	balance = num_bytes;
	index = 0;

	while ( balance > 0 ) {
		tbuf[0]  = '\0';
		if ( balance < 8 )
			j = balance;
		else j = 8;
		for ( i = 0; i < j; ++i ) {
			sprintf(ttbuf,"%02x ",(unsigned char)dbuf[index+i]);
			strcat(tbuf,ttbuf);
		}
		if ( balance < 8 ) {
			filler = 8-balance;
			for ( k = 0; k < filler; ++k ) 
				strcat(tbuf,"   ");
		}
		strcat(tbuf,": ");
		for ( i = 0; i < j; ++i ) {
			if ( !isprint(dbuf[index+i]) )
				strcat(tbuf,". ");
			else {
				sprintf(ttbuf,"%c ",dbuf[index+i]);
				strcat(tbuf,ttbuf);
			}
		}
		sprintf(finalbuf,"%s",tbuf);
		mainwin->lw6_out->addItem(finalbuf);
		balance -= j;
		index += j;
	}
}

static void dump_data6_in(int num_bytes, unsigned char *dbuf)
{
	int i, j, k, index, filler;
	char ttbuf[10];
	char finalbuf[256];
	char tbuf[256];
	int balance = 0;

	balance = num_bytes;
	index = 0;

	while ( balance > 0 ) {
		tbuf[0]  = '\0';
		if ( balance < 8 )
			j = balance;
		else j = 8;
		for ( i = 0; i < j; ++i ) {
			sprintf(ttbuf,"%02x ",(unsigned char)dbuf[index+i]);
			strcat(tbuf,ttbuf);
		}
		if ( balance < 8 ) {
			filler = 8-balance;
			for ( k = 0; k < filler; ++k ) 
				strcat(tbuf,"   ");
		}
		strcat(tbuf,": ");
		for ( i = 0; i < j; ++i ) {
			if ( !isprint(dbuf[index+i]) )
				strcat(tbuf,". ");
			else {
				sprintf(ttbuf,"%c ",dbuf[index+i]);
				strcat(tbuf,ttbuf);
			}
		}
		sprintf(finalbuf,"%s",tbuf);
		mainwin->lw6->addItem(finalbuf);
		balance -= j;
		index += j;
	}
}

//static void get_maxps(int *maxps)
//{
//	int i;
//	bool ok;
//	int iface   = mainwin->label_if->text().toInt(&ok, 10);
//	int aiface  = mainwin->label_aif->text().toInt(&ok, 10);
//
//	char found = 'n';
//	for ( i = 0; i < summ_count; ++i ) {
//	    if ( summ[i].ifnum != iface ) continue;
//	    if ( summ[i].altnum != aiface ) continue;
//	    if ( !(summ[i].eptype & 0x02 ) ) continue;
//	    if ( summ[i].epnum == mainwin->cb6_out->currentText().toInt(&ok, 16) ) {
//		found = 'y';
//		break;
//	    }
//	}
//	if ( found == 'n' ) {
//	      QMessageBox mb;
//	      mb.setText("Endpoint not found!");
//	      mb.exec();
//	      return ;
//	}
//	*maxps = summ[i].maxps;
//} 

static void clearhalt_in()
{
	int r;
	unsigned char ep;
	bool ok;

	ep = mainwin->cb6_in->currentText().toInt(&ok, 16);
	r = cyusb_clear_halt(h, ep);
	if ( r ) {
		libusb_error(r, "Error while automatically clearing halt condition on IN pipe");
		return;
	}
}

static void clearhalt_out()
{
	int r;
	unsigned char ep;
	bool ok;

	ep = mainwin->cb6_out->currentText().toInt(&ok, 16);
	r = cyusb_clear_halt(h, ep);
	if ( r ) {
		libusb_error(r, "Error while automatically clearing halt condition on OUT pipe");
		return;
	}
}

void ControlCenter::on_pb6_rcv_clicked()
{
	int r;
	int transferred = 0;
	bool ok;
	unsigned char *buf;
	char tmpbuf[10];
	int nbw;

	if ( mainwin->cb6_loop->isChecked() ) {
		buf = (unsigned char *)malloc(data_count);
		r = cyusb_bulk_transfer(h, mainwin->cb6_in->currentText().toInt(&ok, 16), buf, 
				data_count, &transferred, 1000);
		printf("Bytes read from device = %d\n",transferred);
		if ( r ) {
			libusb_error(r, "Data Read Error");
			clearhalt_in();
		}
		dump_data6_in(transferred, buf);
		cum_data_in += transferred;
		sprintf(tmpbuf,"%d",cum_data_in);
		mainwin->label6_in->setText(tmpbuf);
		if ( mainwin->le6_infile->text() != "" )
			nbw = write(fd_infile, buf, transferred);
	}
	else {
		buf = (unsigned char *)malloc(mainwin->le6_size->text().toInt(&ok, 10));	      
		r = cyusb_bulk_transfer(h, mainwin->cb6_in->currentText().toInt(&ok, 16), buf, 
				mainwin->le6_size->text().toInt(&ok, 10), &transferred, 1000);
		printf("Bytes read from device = %d\n",transferred);
		dump_data6_in(transferred, buf);
		cum_data_in += transferred;
		sprintf(tmpbuf,"%d",cum_data_in);
		mainwin->label6_in->setText(tmpbuf);
		if ( mainwin->le6_infile->text() != "" )
			nbw = write(fd_infile, buf, transferred);
	}
	free(buf);
}

void ControlCenter::pb6_send_file_selected(unsigned char *buf, int sz)
{
	int r;
	int transferred = 0;
	bool ok;
	char tmpbuf[10];

	r = cyusb_bulk_transfer(h, mainwin->cb6_out->currentText().toInt(&ok, 16), buf, sz, &transferred, 1000);
	printf("Bytes sent to device = %d\n",transferred);
	if ( r ) {
		libusb_error(r, "Error in bulk write!");
		clearhalt_out();
		return ;
	}
	cum_data_out += transferred;
	sprintf(tmpbuf,"%d",cum_data_out);
	mainwin->label6_out->setText(tmpbuf);
	dump_data6_out(transferred, buf);

	if ( mainwin->cb6_loop->isChecked() ) {
		data_count = transferred;
		on_pb6_rcv_clicked();
	}
}

void ControlCenter::pb6_send_nofile_selected()
{
	int r, i;
	int transferred = 0;
	bool ok;
	unsigned char *buf;
	char tmpbuf[10];

	int sz;
	unsigned char val;


	if ( mainwin->le6_out_hex->text() == "" ) {
		sz = mainwin->le6_size->text().toInt(&ok, 10);
		buf = (unsigned char *)malloc(sz);
		if ( mainwin->rb6_constant->isChecked() ) {
			val = mainwin->le6_value->text().toInt(&ok, 16);
			for ( i = 0; i < sz; ++i )
				buf[i] = val;
		}
		else if ( mainwin->rb6_random->isChecked() ) {
			for ( i = 0; i < sz; ++i )
				buf[i] = random();
		}
		else {
			val = mainwin->le6_value->text().toInt(&ok, 16);
			for ( i = 0; i < sz; ++i ) {
				if ( i == 0 )
					buf[i] = val;
				else buf[i] = buf[i-1] + 1;
			}	
		}
	}
	else {
		sz = strlen(qPrintable(mainwin->le6_out_ascii->text() ) + 1);
		buf = (unsigned char *)malloc(sz);
		strcpy((char *)buf, qPrintable(mainwin->le6_out_ascii->text()) );
	}

	cum_data_out = 0;
	cum_data_in  = 0;

	r = cyusb_bulk_transfer(h, mainwin->cb6_out->currentText().toInt(&ok, 16), buf, 
			mainwin->le6_size->text().toInt(&ok, 10), &transferred, 1000);
	printf("Bytes sent to device = %d\n",transferred);
	if ( r ) {
		libusb_error(r, "Error in bulk write!\nPerhaps size > device buffer ?");
		clearhalt_out();
	}
	cum_data_out += transferred;
	sprintf(tmpbuf,"%d",cum_data_out);
	mainwin->label6_out->setText(tmpbuf);

	dump_data6_out(transferred, buf);

	free(buf);

	if ( mainwin->cb6_loop->isChecked() ) {
		data_count = transferred;
		on_pb6_rcv_clicked();
	}
}


void ControlCenter::on_pb6_clearhalt_out_clicked()
{
	int r;
	unsigned char ep;
	bool ok;

	ep = cb6_out->currentText().toInt(&ok, 16);
	r = cyusb_clear_halt(h, ep);
	if ( r ) {
		libusb_error(r, "Error while clearing halt condition on OUT pipe");
		return;
	}
	QMessageBox mb;
	mb.setText("Halt condition cleared on OUT pipe");
	mb.exec();
	return;
}

void ControlCenter::on_pb6_clearhalt_in_clicked()
{
	int r;
	unsigned char ep;
	bool ok;

	ep = cb6_in->currentText().toInt(&ok, 16);
	r = cyusb_clear_halt(h, ep);
	if ( r ) {
		libusb_error(r, "Error while clearing halt condition on IN pipe");
		return;
	}
	QMessageBox mb;
	mb.setText("Halt condition cleared on IN pipe");
	mb.exec();
	return;
}

void ControlCenter::on_pb7_clearhalt_out_clicked()
{
	int r;
	unsigned char ep;
	bool ok;

	ep = cb7_out->currentText().toInt(&ok, 16);
	r = cyusb_clear_halt(h, ep);
	if ( r ) {
		libusb_error(r, "Error while clearing halt condition on OUT pipe");
		return;
	}
	QMessageBox mb;
	mb.setText("Halt condition cleared on OUT pipe");
	mb.exec();
	return;
}

void ControlCenter::on_pb7_clearhalt_in_clicked()
{
	int r;
	unsigned char ep;
	bool ok;

	ep = cb7_in->currentText().toInt(&ok, 16);
	r = cyusb_clear_halt(h, ep);
	if ( r ) {
		libusb_error(r, "Error while clearing halt condition on IN pipe");
		return;
	}
	QMessageBox mb;
	mb.setText("Halt condition cleared on IN pipe");
	mb.exec();
	return;
}

void ControlCenter::on_pb6_send_clicked()
{
	int nbr;
	unsigned char *buf;
	int maxps = 0;

	if ( mainwin->le6_outfile->text() == "" )
		pb6_send_nofile_selected();
	else {
		fd_outfile = open(qPrintable(mainwin->le6_outfile->text()), O_RDONLY);
		if ( fd_outfile < 0 ) {
			QMessageBox mb;
			mb.setText("Output file not found!");
			mb.exec();
			return ;
		}
		if ( mainwin->le6_infile->text() != "" ) {
			fd_infile = open(qPrintable(mainwin->le6_infile->text()), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
			if ( fd_infile < 0 ) {
				QMessageBox mb;
				mb.setText("Input file creation error");
				mb.exec();
				return;
			}
		}

		maxps = 4096;	/* If sending a file, then data is sent in 4096 byte packets */

		buf = (unsigned char *)malloc(maxps);
		while ( (nbr = read(fd_outfile, buf, maxps)) ) {
			pb6_send_file_selected(buf, nbr);
		}
		free(buf);
		::close(fd_outfile);
		::close(fd_infile);
	}
}  


void ControlCenter::on_pb6_selout_clicked()
{
	QString filename;
	filename = QFileDialog::getOpenFileName(this, "Select file to download...", ".", "Any file (*)");
	if ( filename == mainwin->le6_infile->text() ) {
		QMessageBox mb;
		mb.setText("Outfile and Infile cannot be the same !!");
		mb.exec();
		return ;
	}
	else mainwin->le6_outfile->setText(filename);
}

void ControlCenter::on_pb6_selin_clicked()
{
	QString filename;

	if ( mainwin->le6_outfile->text() == "" ) {
		QMessageBox mb;
		mb.setText("Cannot select infile when outfile is blank !");
		mb.exec();
		return;
	}

	mainwin->cb6_loop->setChecked(TRUE);	/* Should auto enable this, for data MUST come only from a file out */

	filename = QFileDialog::getSaveFileName(this, "Select file to write data to...", ".", "Any file (*)");
	if ( filename == mainwin->le6_outfile->text() ) {
		QMessageBox mb;
		mb.setText("Outfile and Infile cannot be the same !!");
		mb.exec();
		return ;
	}
	else mainwin->le6_infile->setText(filename);
}

static void dump_data7_in(int num_bytes, unsigned char *dbuf)
{
	int i, j, k, index, filler;
	char ttbuf[10];
	char finalbuf[256];
	char tbuf[256];
	int balance = 0;

	balance = num_bytes;
	index = 0;

	while ( balance > 0 ) {
		tbuf[0]  = '\0';
		if ( balance < 8 )
			j = balance;
		else j = 8;
		for ( i = 0; i < j; ++i ) {
			sprintf(ttbuf,"%02x ",(unsigned char)dbuf[index+i]);
			strcat(tbuf,ttbuf);
		}
		if ( balance < 8 ) {
			filler = 8-balance;
			for ( k = 0; k < filler; ++k ) 
				strcat(tbuf,"   ");
		}
		strcat(tbuf,": ");
		for ( i = 0; i < j; ++i ) {
			if ( !isprint(dbuf[index+i]) )
				strcat(tbuf,". ");
			else {
				sprintf(ttbuf,"%c ",dbuf[index+i]);
				strcat(tbuf,ttbuf);
			}
		}
		sprintf(finalbuf,"%s",tbuf);
		mainwin->lw7_in->addItem(finalbuf);
		balance -= j;
		index += j;
	}
}

static void dump_data7_out(int num_bytes, unsigned char *dbuf)
{
	int i, j, k, index, filler;
	char ttbuf[10];
	char finalbuf[256];
	char tbuf[256];
	int balance = 0;

	balance = num_bytes;
	index = 0;

	while ( balance > 0 ) {
		tbuf[0]  = '\0';
		if ( balance < 8 )
			j = balance;
		else j = 8;
		for ( i = 0; i < j; ++i ) {
			sprintf(ttbuf,"%02x ",(unsigned char)dbuf[index+i]);
			strcat(tbuf,ttbuf);
		}
		if ( balance < 8 ) {
			filler = 8-balance;
			for ( k = 0; k < filler; ++k ) 
				strcat(tbuf,"   ");
		}
		strcat(tbuf,": ");
		for ( i = 0; i < j; ++i ) {
			if ( !isprint(dbuf[index+i]) )
				strcat(tbuf,". ");
			else {
				sprintf(ttbuf,"%c ",dbuf[index+i]);
				strcat(tbuf,ttbuf);
			}
		}
		sprintf(finalbuf,"%s",tbuf);
		mainwin->lw7_out->addItem(finalbuf);
		balance -= j;
		index += j;
	}
}

static void in_callback( struct libusb_transfer *transfer)
{
	bool ok;
	int pktsin, act_len;
	char tbuf[8];
	unsigned char *ptr;
	int elapsed;
	unsigned char ep_in;
	int pktsize_in;
	char ttbuf[10];
	double inrate;

	printf("Callback function called\n");

	if ( transfer->status != LIBUSB_TRANSFER_COMPLETED ) {
		libusb_error(transfer->status, "Transfer not completed normally");
	}

	totalin = pkts_success = pkts_failure = 0;

	pktsin = mainwin->cb7_numpkts->currentText().toInt(&ok, 10);

	elapsed = isoc_time->elapsed();

	printf("Milliseconds elapsed = %d\n",elapsed);

	for ( int i = 0; i < pktsin; ++i ) {
		ptr = libusb_get_iso_packet_buffer_simple(transfer, i);
		act_len = transfer->iso_packet_desc[i].actual_length;
		totalin += 1;
		if ( transfer->iso_packet_desc[i].status == LIBUSB_TRANSFER_COMPLETED ) {
			pkts_success += 1;
			if ( mainwin->rb7_enable->isChecked() ) {
				dump_data7_in(act_len, ptr);
				mainwin->lw7_in->addItem("");
			}
		}
		else pkts_failure += 1;
	}
	sprintf(tbuf,"%6d",totalin);
	mainwin->label7_totalin->setText(tbuf);
	sprintf(tbuf,"%6d",pkts_success);
	mainwin->label7_pktsin->setText(tbuf);
	sprintf(tbuf,"%6d",pkts_failure);
	mainwin->label7_dropped_in->setText(tbuf);
	ep_in = mainwin->cb7_in->currentText().toInt(&ok, 16);  
	pktsize_in = cyusb_get_max_iso_packet_size(h, ep_in);
	inrate = ( (((double)totalin * (double)pktsize_in) / (double)elapsed ) * (1000.0 / 1024.0) );
	sprintf(ttbuf, "%8.1f", inrate);
	mainwin->label7_ratein->setText(ttbuf);
}	

static void out_callback( struct libusb_transfer *transfer)
{
	bool ok;
	int pktsout, act_len;
	char tbuf[8];
	unsigned char *ptr;
	int elapsed;
	unsigned char ep_out;
	int pktsize_out;
	char ttbuf[10];
	double outrate;


	printf("Callback function called\n");

	if ( transfer->status != LIBUSB_TRANSFER_COMPLETED ) {
		libusb_error(transfer->status, "Transfer not completed normally");
	}

	totalout = pkts_success = pkts_failure = 0;

	pktsout = mainwin->cb7_numpkts->currentText().toInt(&ok, 10);

	elapsed = isoc_time->elapsed();

	printf("Milliseconds elapsed = %d\n",elapsed);

	for ( int i = 0; i < pktsout; ++i ) {
		ptr = libusb_get_iso_packet_buffer_simple(transfer, i);
		act_len = transfer->iso_packet_desc[i].actual_length;
		totalout += 1;
		if ( transfer->iso_packet_desc[i].status == LIBUSB_TRANSFER_COMPLETED ) {
			pkts_success += 1;
			if ( mainwin->rb7_enable->isChecked() ) {
				dump_data7_out(act_len, ptr);
				mainwin->lw7_out->addItem("");
			}
		}
		else pkts_failure += 1;
	}
	sprintf(tbuf,"%6d",totalout);
	mainwin->label7_totalout->setText(tbuf);
	sprintf(tbuf,"%6d",pkts_success);
	mainwin->label7_pktsout->setText(tbuf);
	sprintf(tbuf,"%6d",pkts_failure);
	mainwin->label7_dropped_out->setText(tbuf);
	ep_out = mainwin->cb7_out->currentText().toInt(&ok, 16);  
	pktsize_out = cyusb_get_max_iso_packet_size(h, ep_out);
	outrate = ( (((double)totalout * (double)pktsize_out) / (double)elapsed ) * (1000.0 / 1024.0) );
	sprintf(ttbuf, "%8.1f", outrate);
	mainwin->label7_rateout->setText(ttbuf);
}	

void ControlCenter::on_pb7_rcv_clicked()
{
	int numpkts;	
	bool ok;
	int pktsize_in;
	int bufsize_in;
	unsigned char ep_in;
	int r;
	char tbuf[10];
	struct timeval tv;

	if ( cb7_in->currentText() == "" ) {  /* No ep_in exists */
		QMessageBox mb;
		mb.setText("Cannot receive data when no input endpoint present");
		mb.exec();
		return;
	} 

	ep_in = cb7_in->currentText().toInt(&ok, 16);  
	pktsize_in = cyusb_get_max_iso_packet_size(h, ep_in);
	sprintf(tbuf,"%9d",pktsize_in);
	mainwin->label7_pktsize_in->setText(tbuf);

	numpkts = mainwin->cb7_numpkts->currentText().toInt(&ok, 10);
	bufsize_in = pktsize_in * numpkts;
	isoc_databuf = (unsigned char *)malloc(bufsize_in);

	transfer = libusb_alloc_transfer(numpkts);
	if ( !transfer ) {
		QMessageBox mb;
		mb.setText("Alloc failure");
		mb.exec();
		return;
	}

	libusb_fill_iso_transfer(transfer, h, ep_in, isoc_databuf, bufsize_in, numpkts, in_callback, NULL, 10000 );

	libusb_set_iso_packet_lengths(transfer, pktsize_in);
	transfer->flags = LIBUSB_TRANSFER_FREE_BUFFER | LIBUSB_TRANSFER_FREE_TRANSFER;

	isoc_time = new QTime();
	isoc_time->start();

	r = libusb_submit_transfer(transfer);

	tv.tv_sec = 0;
	tv.tv_usec = 50;

	for ( int i = 0; i < 100; ++i ) {
		libusb_handle_events_timeout(NULL,&tv);
	}

	if ( r ) {
		printf("Error %d submitting transfer\n", r);
	}
}

void ControlCenter::on_pb7_send_clicked()
{
	int numpkts;	
	bool ok;
	int pktsize_out;
	int bufsize_out;
	unsigned char ep_out;
	int r;
	char tbuf[10];
	struct timeval tv;

	if ( cb7_out->currentText() == "" ) {  /* No ep_out exists */
		QMessageBox mb;
		mb.setText("Cannot send data when no output endpoint present");
		mb.exec();
		return;
	} 

	ep_out = cb7_out->currentText().toInt(&ok, 16);  
	pktsize_out = cyusb_get_max_iso_packet_size(h, ep_out);
	sprintf(tbuf,"%9d",pktsize_out);
	mainwin->label7_pktsize_out->setText(tbuf);

	numpkts = mainwin->cb7_numpkts->currentText().toInt(&ok, 10);
	bufsize_out = pktsize_out * numpkts;
	isoc_databuf = (unsigned char *)malloc(bufsize_out);

	for ( int i = 0; i < numpkts; ++i )
		for ( int j = 0; j < pktsize_out; ++j )
			isoc_databuf[i*pktsize_out+j] = i + 1;

	transfer = libusb_alloc_transfer(numpkts);
	if ( !transfer ) {
		QMessageBox mb;
		mb.setText("Alloc failure");
		mb.exec();
		return;
	}

	libusb_fill_iso_transfer(transfer, h, ep_out, isoc_databuf, bufsize_out, numpkts, out_callback, NULL, 10000 );

	libusb_set_iso_packet_lengths(transfer, pktsize_out);
	transfer->flags = LIBUSB_TRANSFER_FREE_BUFFER | LIBUSB_TRANSFER_FREE_TRANSFER;

	isoc_time = new QTime();
	isoc_time->start();

	r = libusb_submit_transfer(transfer);


	tv.tv_sec = 0;
	tv.tv_usec = 50;

	for ( int i = 0; i < 100; ++i ) {
		libusb_handle_events_timeout(NULL,&tv);
	}

	if ( r ) {
		printf("Error %d submitting transfer\n", r);
	}
}

void ControlCenter::on_pb7_clear_clicked()
{
	mainwin->label7_pktsize_out->clear();
	mainwin->label7_pktsize_in->clear();
	mainwin->label7_totalout->clear();
	mainwin->label7_totalin->clear();
	mainwin->label7_pktsout->clear();
	mainwin->label7_pktsin->clear();
	mainwin->label7_dropped_out->clear();
	mainwin->label7_dropped_in->clear();
	mainwin->label7_rateout->clear();
	mainwin->label7_ratein->clear();
	mainwin->lw7_out->clear();
	mainwin->lw7_in->clear();
}

void ControlCenter::on_rb7_enable_clicked()
{
	mainwin->lw7_out->setEnabled(TRUE);
	mainwin->lw7_in->setEnabled(TRUE);
}

void ControlCenter::on_rb7_disable_clicked()
{
	mainwin->lw7_out->setEnabled(FALSE);
	mainwin->lw7_in->setEnabled(FALSE);
}

static int multiple_instances()
{

	if ( server.listen("/dev/shm/cyusb_linux") ) {
		return 0;   /* Only one instance of this application is running  */
	}

	/* If I am here, then EITHER the application is already runnning ( most likely )
	   OR the socket file already exists because of an earlier crash ! */
	return 1;
}

void ControlCenter::appExit()
{
	exit(0);
}

void ControlCenter::about()
{
	QMessageBox mb;
	mb.setText("CyUSB Suite for Linux - Version 1.0");
	mb.setDetailedText("(c) Cypress Semiconductor Corporation, ATR-LABS - 2012");
	mb.exec();
	return;
}

int main(int argc, char **argv)
{
	int r;

	QApplication::setStyle(new QCleanlooksStyle);
	QApplication app(argc, argv);

	if ( multiple_instances() ) {
		printf("Application already running ? If NOT, manually delete socket file /dev/shm/cyusb_linux and restart\n");
		return -1;
	}

	r = cyusb_open();
	if ( r < 0 ) {
		printf("Error opening library\n");
		return -1;
	}
	else if ( r == 0 ) {
		printf("No device found\n");
	}
	else num_devices_detected = r;

	signal(SIGUSR1, setup_handler);

	mainwin = new ControlCenter;
	QMainWindow *mw = new QMainWindow(0);
	mw->setCentralWidget(mainwin);
	QIcon *qic = new QIcon("cypress.png");
	app.setWindowIcon(*qic);
	set_tool_tips();
	mw->setFixedSize(880, 660);

	update_devlist();

	sb = mw->statusBar();

	QAction *exitAct  = new QAction("e&Xit", mw);
	QAction *aboutAct = new QAction("&About", mw);

	QMenu *fileMenu = new QMenu("&File");
	QMenu *helpMenu = new QMenu("&Help");
	fileMenu->addAction(exitAct);
	helpMenu->addAction(aboutAct);
	QMenuBar *menuBar = new QMenuBar(mw);
	menuBar->addMenu(fileMenu);
	menuBar->addMenu(helpMenu);
	QObject::connect(exitAct, SIGNAL(triggered()), mainwin, SLOT(appExit()));
	QObject::connect(aboutAct,SIGNAL(triggered()), mainwin, SLOT(about()));

	mw->show();

	sb->showMessage("Starting Application...",2000);

	return app.exec();
}
