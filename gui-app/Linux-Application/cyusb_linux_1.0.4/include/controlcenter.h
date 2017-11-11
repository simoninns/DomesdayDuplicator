#include <QtCore>

#include <sys/types.h>
#include <sys/socket.h>

#ifndef CONTROLCENTERFORM_H
#define CONTROLCENTER_H

#include "ui_controlcenter.h"

class ControlCenter : public QWidget, public Ui::ControlCenter
{
	Q_OBJECT

public:
	ControlCenter(QWidget *parent = 0);
	static void unixhandler_sigusr1(int unused);
	QSocketNotifier *sn_sigusr1;

public slots:
	void on_pb_setIFace_clicked();
	void on_pb_setAltIf_clicked();
	void on_pb6_rcv_clicked();
	void pb6_send_nofile_selected();
	void pb6_send_file_selected(unsigned char *, int);
	void on_pb7_send_clicked();
	void appExit();
	void about();

private slots:
	void on_pb_kerneldetach_clicked();
	void on_listWidget_itemClicked(QListWidgetItem *item);

	void sigusr1_handler();

	void on_pb1_selfile_clicked();
	void on_pb1_start_clicked();
	void on_rb1_ram_clicked();
	void on_rb1_small_clicked();
	void on_rb1_large_clicked();
	void on_pb_reset_clicked();
	void on_rb3_ramdl_clicked();
	void on_rb3_ramup_clicked();
	void on_rb3_eedl_clicked();
	void on_rb3_eeup_clicked();
	void on_rb3_getchip_clicked();
	void on_rb3_renum_clicked();
	void on_rb3_custom_clicked();
	void on_rb3_out_clicked();
	void on_rb3_in_clicked();
	void on_pb3_selfile_clicked();
	void on_pb_execvc_clicked();
	void on_pb3_dl_clicked();
	void on_le3_out_hex_textEdited();
	void on_le3_out_ascii_textEdited();
	void on_le3_out_ascii_textChanged();
	void on_pb4_selfile_clicked();
	void on_pb4_start_clicked();
	void on_pb4_clear_clicked();
	void on_le6_out_hex_textEdited();
	void on_le6_out_ascii_textEdited();
	void on_le6_out_ascii_textChanged();
	void on_pb6_clear_clicked();
	void on_rb6_constant_clicked();
	void on_rb6_random_clicked();
	void on_rb6_inc_clicked();
	void on_pb6_send_clicked();
	void on_cb6_loop_clicked();
	void on_pb6_selout_clicked();
	void on_pb6_selin_clicked();
	void on_pb6_clearhalt_out_clicked();
	void on_pb6_clearhalt_in_clicked();
	void on_pb7_clear_clicked();
	void on_pb7_rcv_clicked();
	void on_rb7_enable_clicked();
	void on_rb7_disable_clicked();
	void on_pb7_clearhalt_out_clicked();
	void on_pb7_clearhalt_in_clicked();
};

#endif
