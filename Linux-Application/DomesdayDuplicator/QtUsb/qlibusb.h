#ifndef QLIBUSB_H
#define QLIBUSB_H

#include "qbaseusb.h"
#include "qusb_compat.h"
#include "qusbbulktransfer.h"
#include <QByteArray>
#include <QDebug>
#include <QString>
#include <QtEndian>
#include <libusb-1.0/libusb.h>

/**
 * @brief
 *
 */
class QUSBSHARED_EXPORT QUsbDevice : public QBaseUsbDevice {
  Q_OBJECT

public:
  /**
   * @brief See base class
   *
   * @param parent
   */
  explicit QUsbDevice(QBaseUsbDevice *parent = 0);
  /**
   * @brief See base class
   *
   */
  ~QUsbDevice();

  /**
   * @brief See base class
   *
   * @return QtUsb::FilterList
   */
  static QtUsb::FilterList getAvailableDevices(void);

public slots:
  /**
   * @brief See base class
   *
   * @return qint32
   */
  qint32 open();
  /**
   * @brief See base class
   *
   */
  void close();

  /**
   * @brief See base class
   *
   */
  void flush();
  /**
   * @brief See base class
   *
   * @param buf
   * @param maxSize
   * @return qint32
   */
  qint32 read(QByteArray *buf, quint32 len);
  /**
   * @brief See base class
   *
   * @param buf
   * @param maxSize
   * @return qint32
   */
  qint32 write(const QByteArray *buf, quint32 len);

  qint32 sendControlTransfer(
      quint8                 bmRequestType,
      quint8                 bRequest,
      quint16                wValue,
      quint16                wIndex,
      const QByteArray*      data,
      quint16                wLength,
      quint16                timeout);

  qint32 startBulkTransfer(bool testMode);
  qint32 stopBulkTransfer(void);

  quint32 getSuccessCounter(void);
  quint32 getFailureCounter(void);
  quint32 getTransferPerformance(void);

  /**
   * @brief See base class
   *
   * @param enable
   */
  void setDebug(bool enable);

private slots:

private:
  /**
   * @brief Print error code to qWarning
   *
   * @param error_code
   */
  void printUsbError(int error_code);
  libusb_device **mDevs;            /**< libusb device ptr to ptr */
  libusb_device_handle *mDevHandle; /**< libusb device handle ptr */
  libusb_context *mCtx;             /**< libusb context */
  QByteArray mReadBuffer;
  quint32 mReadBufferSize;
  QUsbBulkTransfer* mUsbBulkTransfer;
};
#endif // QLIBUSB_H
