#include <QtCore>
#include <QtGui>

#include <sys/types.h>
#include <sys/socket.h>

#include "../include/controlcenter.h"

int sigusr1_fd[2];

ControlCenter::ControlCenter(QWidget *parent) : QWidget(parent)
{
	setupUi(this);

	if ( socketpair(AF_UNIX, SOCK_STREAM, 0, sigusr1_fd) )
	   qFatal("Couldn't create SIGUSR1 socketpair");

	sn_sigusr1 = new QSocketNotifier(sigusr1_fd[1], QSocketNotifier::Read, this);
	connect(sn_sigusr1, SIGNAL(activated(int)), this, SLOT(sigusr1_handler()));

	QStringList list;
	list.clear();
	list << "1" << "2" << "4" << "8" << "16" << "32" << "64" << "128";
	cb7_numpkts->addItems(list);
	rb7_enable->setChecked(FALSE); /* disabled by default for maximum performance */
	rb7_disable->setChecked(TRUE); /* disabled by default for maximum performance */
}
