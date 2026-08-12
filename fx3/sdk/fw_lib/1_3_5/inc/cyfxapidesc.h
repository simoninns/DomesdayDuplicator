/*
 ## Cypress FX3 Core Library Header (cyfxapidesc.h)
 ## ===========================
 ##
 ##  Copyright Cypress Semiconductor Corporation, 2010-2023,
 ##  All Rights Reserved
 ##  UNPUBLISHED, LICENSED SOFTWARE.
 ##
 ##  CONFIDENTIAL AND PROPRIETARY INFORMATION
 ##  WHICH IS THE PROPERTY OF CYPRESS.
 ##
 ##  Use of this file is governed
 ##  by the license agreement included in the file
 ##
 ##     <install>/license/license.txt
 ##
 ##  where <install> is the Cypress software
 ##  installation root directory path.
 ##
 ## ===========================
 */

#ifndef _INCLUDED_CYFXAPIDESC_H_
#define _INCLUDED_CYFXAPIDESC_H_

/** \file cyfxapidesc.h
    \brief Provides a brief description of the FX3 APIs.
 */

/** \mainpage EZ-USB FX3 Api Reference Guide
    \tableofcontents

    \section FX3Intro Introduction
    \brief The EZ-USB family includes multiple general purpose USB peripheral
    controllers that provide the advantages of high performance and flexible usage
    models to system designers. The FX3, FX3S and CX3 devices are the latest additions
    to this family, and provide the high performance USB 3.0 SuperSpeed capability
    in addition to a variety of industry standard peripheral interfaces.

    <B>Description</B>\n
    Cypress EZ-USB FX3 is the next generation USB 3.0 peripheral controller
    providing highly integrated and flexible features that enable developers to
    add USB 3.0 functionality to any system.

    It has a fully configurable, parallel, General Programmable Interface called
    GPIF II, which can connect to any processor, ASIC, DSP, or FPGA. It has an
    integrated USB PHY and controller along with a 32-bit micro-controller
    (ARM926EJ-S) for powerful data processing and for building custom applications.

    The FX3S device is an extension to the FX3 device that adds the capability
    to control SD/eMMC/SDIO peripherals to the feature set supported by the FX3.

    The EZ-USB CX3 controller is an extension to the EZ-USB FX3 device which adds 
    the ability to interface with and perform uncompressed video transfers from 
    Image Sensors implementing the MIPI CSI-2 interface. 

    These devices have an integrated inter-port DMA architecture which enables data
    transfers at speeds greater than 400 MBps (Upto 300MBps for CX3). An integrated 
    USB 2.0 OTG controller (on FX3/FX3S) enables applications that need dual role 
    usage scenarios. There is up to 512 KB of on-chip SRAM for code and data usage.

    There are also serial peripherals such as UART, SPI, I2C, I2S and GPIO for
    communicating with on board peripherals.

    \section Fx3SDKIntro EZ-USB FX3 SDK
    \brief The EZ-USB FX3 SDK is a full featured firmware solution that allows
    customers to leverage the features of the FX3/FX3S/CX3 devices, and build a full
    featured design in a short time.

    <B>Description</B>\n
    The FX3 SDK provides a firmware framework that allows FX3/FX3S/CX3 customers to
    quickly complete the firmware part of their system design. The firmware framework
    provides a set of drivers and convenience APIs for the various hardware blocks
    on these devices. The drivers abstract the complexity of the FX3 device architecture
    and provide easy-to-use API interfaces for customers to configure and control
    the FX3 device operation.

    \subsection Fx3Lib FX3 API Libraries
    \brief The FX3 SDK provides a set of libraries which provide APIs to configure and
    control the operation of the FX3 device.
    
    <B>Description</B>\n
    The FX3 SDK provides a firmware framework which includes an embedded RTOS,
    drivers for each of the FX3 device blocks and a set of APIs that configure and
    control the operation of the device. These APIs abstract out the complexity of
    the device hardware and allows users to focus on the core application logic.

    The drivers and API are structured in the form of multiple static libraries that
    adhere to the ARM EABI conventions; and can be linked in to application built using
    compiler toolchains such as gcc that support the EABI standard.

    The following libraries are part of the SDK release and can be found under the
    firmware/u3p_firmware/lib folder in the installation:

      - cyu3threadx.a : This library provides the ThreadX Operating System from
        Express Logic. All of the Operating System functionality is enabled and the
        licensing terms allow users to use the OS services free of cost.
      - cyfxapi.a     : This library provides the drivers and APIs for the core FX3
        device functionality including memory, cache and interrupt control, DMA, USB
        and GPIF blocks.
      - cyu3lpp.a     : This library provides the drivers and APIs for the serial
        peripheral blocks (UART, SPI, I2C, I2S and GPIO) on the FX3 device. The sources
        for this library are also provided and can be customized as needed.
      - cyu3sport.a   : This library provides the drivers and APIs for the Storage
        (SD/MMC/SDIO) interface on the FX3S device. This library only needs to be used
        when building storage enabled applications using the FX3S device.
      - cyu3mipicsi.a : This library provides the APIs for the MIPI-CSI2 interface on the
        CX3 device. This library only needs to be used when building Image Sensor 
        applications on the CX3 which use the CX3 MIPI-CSI2 interface.

    The API definitions for all of these libraries is provided in the header files under
    the firmware/u3p_firmware/inc folder in the SDK installation.

    \subsection Fx3BootApi FX3 Boot API Library
    \brief The FX3 SDK also provides a low footprint API library which supports USB, SPI,
    I2C, UART and GPIO interfaces. This library can be used to built custom boot-loaders
    or minimal FX3 applications.

    <B>Description</B>\n
    Some FX3 based systems may need to use a custom boot-loader due to one of the following
    reasons:
      -# The system needs a custom set of USB descriptors (with customer specific VID/PID)
         when booting from the USB host.
      -# The full FX3 application is too large to fit on the EEPROM/Flash device used for
         booting.

    The SDK provides a separate API library to facilitate such applications. This library
    only supports USB device mode, I2C, SPI, UART and GPIO (simple only) operations. DMA
    support is limited and covers only transfer of single data buffers into or out of the
    device.

    These APIs are provided as a separate static library which has no connection with the
    libraries described above. It is not possible to use both sets of libraries in a single
    FX3 application.

    When using the Boot API to implement a USB based boot-loader application, it is possible
    to transfer control from the boot application to the full application and back without
    going through USB re-enumeration. This method allows seamless firmware upgrades without
    the need to go through multiple driver binding steps on the host side.

    The library and header files for the boot API are found under the firmware/boot_fw/lib
    and firmware/boot_fw/include folders in the SDK installation.

    \subsection Fx3Examples FX3 Application Examples
    \brief The FX3 SDK includes a set of application examples which use the FX3 API to
    demonstrate various types of data transfer.

    <B>Description</B>\n
    In addition to these driver and API libraries, the FX3 SDK also provides a set of
    application examples. These examples demonstrate how the APIs can be used to put
    together applications. The main focus of these examples is the USB and DMA
    configuration which forms the core of most FX3 applications. Specific examples
    demonstrate the use of the GPIF, Serial Peripheral and Storage APIs.
    
    Please refer to the FX3 Programmer's Manual for details on the various application
    examples.

    \section RtosLib Embedded Real Time Operating System
    \brief The FX3 SDK provides a full-featured embedded Real-Time Operating System (RTOS)
    which can be used to create complex multi-threaded applications.

    <B>Description</B>\n
    The FX3 firmware framework makes use of a Embedded Real-Time Operating System.
    The drivers for various peripheral blocks in the platform are implemented as
    separate threads and other OS services such as Semaphores, Message Queues,
    Mutexes and Timers are used for inter-thread communication and task synchronization.

    All of the RTOS services are made available to the user application through a set
    of wrapper functions that abstract out the actual RTOS API calls. This allows
    users to implement their applications using multiple communicating threads with
    a full set of IPC mechanisms for synchronization and data protection.

    The ThreadX operating system from Express Logic is used as the embedded RTOS in the
    FX3 device. All of the functionality supported by the ThreadX OS is made available
    for use by the application logic. Some constraints on their use are placed to ensure
    the smooth functioning of all of the drivers.

    **\see\n
     *\see CyU3PThread
     *\see CyU3PQueue
     *\see CyU3PMutex
     *\see CyU3PSemaphore
     *\see CyU3PEvent
     *\see CyU3PTimer
     *\see CyU3PBytePool
     *\see CyU3PBlockPool

    \section DMAMgr DMA Management
    \brief The DMA manager in the firmware library provides functions to create,
    configure, control and operate DMA channels within the FX3 device.

    <B>Description</B>\n
    The FX3 device architecture includes a DMA fabric that is used to steer data
    between various interfaces of the device.  The DMA manager provides a set
    of functions that allow the user application to create data flows between
    any pair of interfaces, to configure its behavior and to steer or monitor
    the actual flow of data packets on this path.

    \subsection DmaSock DMA Sockets
    \brief A DMA socket is a FX3 device construct that maps to one end of a data path
    into or out of the device.

    <B>Description</B>\n
    Each interface of the FX3 device (such as USB or Processor port) can support
    multiple independent data flows into or out of the device through that port.
    Each data flow on a port is handled by a hardware block called a DMA socket.
    Each port supports a device defined number or sockets all of which can
    function independently of each other.

    For example, different sockets on the Processor port will be used to handle
    the incoming data going to the storage device and to handle the outgoing data
    that originated from a USB endpoint.

    A socket that takes data coming in from an external interface and directs it
    into the FX3 system memory is called a Producer or Ingress socket. A socket
    through which data from the system memory goes out of the FX3 device is called
    a Consumer or Egress socket.

    **\see\n
     *\see CyU3PDmaSocket_t

    \subsection DmaBuff DMA Buffers
    \brief A DMA buffer is a memory buffer in the FX3 system memory that is assigned
    to hold one or more packets of data in a data flow.

    <B>Description</B>\n
    All data flowing through the FX3 device passes through the system memory.
    i.e., All data packets being streamed from a USB endpoint to the external
    Processor on the P-port pass through designated buffers in the FX3 system
    memory.  Depending on the type of data path and the bandwidth requirements,
    each data flow might require different sizes and number of buffers in the
    device memory.

    \subsection DmaDesc DMA Descriptors
    \brief A DMA descriptor is a data structure that binds the buffers and sockets
    corresponding to a data flow.

    <B>Description</B>\n
    The DMA fabric in the FX3 device makes use of data structures called as
    DMA descriptors to bind the DMA buffers used for a data flow with the DMA
    sockets that form its ends.  Each descriptor contains information on the
    location, size and status of one DMA buffer along with the ids of its
    producer and consumer sockets.

    **\see\n
     *\see CyU3PDmaDescriptor_t

    \subsection DmaChn DMA Channels
    \brief A DMA channel is a software construct that encapsulates all of the
    hardware elements that are used in a single data flow.

    <B>Description</B>\n
    The DMA manager in the FX3 firmware library introduces the notion of a DMA
    channel that encapsulates the hardware resources such as sockets, buffers
    and descriptors used for handling a data flow through the device. The
    channel concept is used to hide the complexity of configuring all of these
    resources in a consistent manner. The DMA manager provides API functions
    that can be used to create data flows between any two interfaces on the
    FX3 device.

    The relationship between the DMA sockets, descriptors and buffers in a DMA
    channel is shown below.
    \image html sock_descr.jpg
    \image latex sock_descr.eps "Resource linkage in a DMA channel" width=16cm

    Since different applications can require different levels of firmware
    involvement in the processing of the data flow, the DMA manager supports
    multiple channel types that require different levels of software intervention.

    The following channel types are supported:
      - Simple Channels\n
        These are DMA channels that bind one producer socket with one consumer socket.
        That is, the data flows from one socket to another in the case of Simple DMA
        channels.
        -# Auto Channel\n
           An Auto channel is one where there is no software involvement in the
           processing of individual data packets. A channel of this type can be created,
           configured to transfer a specified amount of data and then left alone. The
           DMA manager will come back with a notification once the specified data transfer
           has been completed. It is possible to set the transfer length as infinite, if
           these notifications are not required.
        -# Auto Channel with Signalling\n
           This is a special case of the Auto channel where the DMA manager provides
           notifications for every data packet that is handled. The notifications cannot
           be used to control the data flow because the packets would already have been
           forwarded to the consumer, but can be used for statistics collection or for
           error checks.
        -# Manual Channel\n
           A manual channel is one which requires software intervention for the handling
           of each individual data packet. The user application will be notified on
           arrival of each data packet from the producer. The application can make
           desired changes to the data packet (content as well as size changes) before
           sending it on to the consumer. The throughput obtained on a manual channel
           will depend on the amount of processing done by the application on each packet.
        -# Manual IN Channel\n
           This channel type is used for data flows from an external interface to the
           application running on the FX3 CPU. The application gets notified on the
           arrival or each data packet and can use this to read and analyze the packet
           contents.
        -# Manual OUT Channel\n
           This channel type is used when the application running on the FX3 CPU needs
           to send data out to an external device. The application can generate the
           data packets one at a time, and call a function to have it sent out through
           the specified socket. It is possible for the application to create a synthetic
           Manual channel by combining a Manual IN channel and a Manual OUT channel with
           suitable processing.
      - Multi-Channels\n
        Multi-channels are specialized versions of DMA channels that involve either
        multiple producers or multiple consumers. Based on whether there are multiple
        producers or multiple consumers, these can be designated as many-to-one or
        one-to-many channels. Data transfer will be performed in an interleaved fashion.
        Multi-channels can be auto or manual channels based on the operating model.
        -# One-to-Many Auto Channel\n
           This Multi-Channel type has one producer and two or more consumers connected
           in an interleaved manner. The first buffer of data received by the producer
           is sent to the first consumer, the second buffer to the second consumer; and
           so on. As this is an AUTO channel, the user sets up the transfer size and gets
           notified when the specified amount of data has been transferred by the producer.
        -# One-to-Many Manual Channel\n
           This is similar to the One-to-Many AUTO channel, but operates in manual mode.
           Produce event notifications are provided whenever the producer has received
           a buffer of data. A commit operation sends the data out through the next consumer
           socket in the interleaved sequence.
        -# Many-to-One Auto Channel\n
           The Many-to-One channel type connects multiple producer sockets to a single
           consumer socket. In this case, data received through multiple sockets is
           aggregated into a single flow and sent out through a consumer socket. The channel
           operating model is AUTO and event notifications are provided only when the desired
           amount of data has been transferred by the consumer socket.
        -# Many-to-One Manual Channel\n
           This is a MANUAL version of the Many-to-One channel, where the user application
           is notified when data is received by any one of the producer sockets. Even if
           data is received out of order by the various sockets, it will only be sent out
           in the correct sequence which is defined when creating the DMA channel.
        -# Multicast Channel\n
           This is a special one to many channel where there is one producer and a
           maximum of four consumers. The data received on the producer is sent to each
           of the consumers. The channel type only supports a manual mode of operation.

    **\see\n
     *\see CyU3PDmaType_t
     *\see CyU3PDmaChannel
     *\see CyU3PDmaMultiType_t
     *\see CyU3PDmaMultiChannel

    \section Fx3DebugLog Logging Support
    \brief This section documents the APIs that are provided to facilitate logging
    of firmware activity for debug purposes.

    <B>Description</B>\n
    The FX3 API library includes a provision to log firmware actions and send
    them out to a selected target such as the UART console. The logs can be
    generated by the API library or the drivers themselves, or be messages sent
    by the application logic.

    The log messages are classified into two types:\n
    1. Codified messages: These messages contain only a two byte message ID and
       a four byte parameter. This kind of messages are used to compress the log
       output; and require an external decoder to interpret the logs based on the
       message ID.\n
    2. Verbose messages: These are free-form string messages. A function with a
       signature similar to printf is provided for generating these messages.\n

    The log messages are further classified based on priority levels ranging from
    0 to 9. 0 is the highest priority and 9 is the lowest. The logger
    implementation allows the user to set a priority threshold value at runtime,
    and only messages with a higher priority will be logged.

    All log messages will include a source ID which identifies the thread that
    generated the message, the priority assigned to the message and the message
    ID. In case the message is a codified one, the message ID can range from
    0x0000 to 0xFFFE; and the message will also contain a 4 byte parameter field.

    If the message is a verbose one, the message ID will be set to 0xFFFF and
    the parameter will indicate the length of the message string. The next length
    bytes will form the actual text of the string.

    **\see\n
     *\see CyU3PDebugInit
     *\see CyU3PDebugPrint
     *\see CyU3PDebugLog

    \section UsbApi USB Driver and API
    \brief The USB driver and APIs allow users to configure the USB functionality of
    the FX3 device in peripheral as well as OTG/Host mode of operation.
    
    <B>Description</B>\n
    The USB block on the FX3 device is capable of functioning as a USB 3.0
    peripheral, a USB 2.0 peripheral or a USB 2.0 host. It also supports USB 2.0 OTG
    functionality and Battery Charger detection.

    The USB driver takes care of the device operation in each of these modes, and
    handles data transfers in a USB specification compliant manner. The USB APIs allow
    the user to configure the USB block and the driver in the appropriate mode
    as well as configure the device endpoints as required.

    \subsection UsbDevMode USB Peripheral (Device) Mode
    \brief The most common operating mode of the USB block is that of a peripheral
    device. The USB API allows the user to customize the USB peripheral functionality
    as required.

    <B>Description</B>\n
    The USB driver and API control the operation of the FX3 device as a USB
    peripheral in USB SuperSpeed, Hi-Speed and Full Speed modes. The driver detects
    the connection speed and takes care of providing the appropriate USB descriptors
    as well as setting the endpoint parameters as required. This ensures that a fully
    USB specification compliant application can be implemented by following the few
    implementation guidelines.

    In the default operating mode, the FX3 detects the voltage on the VBus input and
    attempts to connect to the host only when a valid VBus voltage is detected. It
    also disables the USB connection and turns off the PHY block when the VBus voltage
    is removed.

    **\see\n
     *\see CyU3PUsbStart
     *\see CyU3PConnectState

    \subsection UsbHostMode USB 2.0 Host Mode
    \brief The FX3 device supports a programmable USB 2.0 host implementation that can
    control a single USB Hi-Speed, Full speed or Low speed peripheral. The USB host API
    in the FX3 SDK allow the user to control the host port and to perform transfers on
    the control and data pipes.

    <B>Description</B>\n
    When functioning as a USB 2.0 host, the FX3 device supports a single root port which
    can be connected to any USB 2.0 device (hubs are not supported). The USB host mode
    API requires that the USB driver be placed into a specific host mode of operation.
    If a dual role device implementation is required, refer to the OTG section of the USB
    API.

    The host API provides functions to control the root port as well as to configure/control
    the connected peripheral device. Both periodic (isochronous and interrupt) as well as
    asynchronous (control and bulk) transfers are supported. The APIs provided help
    implementation of raw data transfers from/to the device endpoints, and does not
    provide an USB host stack or class driver support.

    **\see\n
     *\see CyU3PUsbHostStart

    \subsection UsbOtgMode USB On-The-Go (OTG) Mode
    \brief The USB API supports the use of FX3 as a OTG A or B device, and supports the
    standard OTG Session Request (SRP) and Host Negotiation (HNP) protocols.

    <B>Description</B>\n
    If FX3 is to be used as a dual-role USB On-The-Go (OTG) device, the USB driver needs
    to be initialized for the OTG mode of operation. In this case, the driver monitors the
    state of the ID pin and performs the following functions:
      - Performs ACA charger detection as per the USB Batter Charging Specification 1.1.
      - Detects connection type (A device or B device) and sets up the device accordingly.

    The OTG APIs also provide functions to perform the Session Request Protocol (SRP) for
    requesting a VBus supply; and the Host Negotiation Protocol (HNP) to initiate a device
    role change.

    **\see\n
     *\see CyU3POtgMode_t
     *\see CyU3POtgStart

    \section LPP Serial Peripheral Interfaces
    \brief The serial peripheral interfaces and the corresponding API in
    the FX3 library support data exchange between the FX3 device and external
    peripherals or controllers that are connected through standard peripheral
    interfaces.

    <B>Description</B>\n
    The FX3 device supports a set of serial peripheral interfaces that can be
    used to connect a variety of slave or peer devices to FX3. The peripheral
    interfaces supported by the device are:
      -# UART
      -# I2C
      -# SPI
      -# I2S
      -# GPIOs

    The I2C, SPI and I2S interface implementations on the FX3 device are master
    mode only and can talk to a variety of slave devices. The I2C interface is
    also capable of functioning in multi-master mode where other I2C master
    devices are present on the bus.

    \subsection UartIntf UART Interface
    \brief The UART API allows users to configure the UART interface parameters and
    to setup UART data transfers.
    
    <B>Description</B>\n
    The UART interface on the FX3 device can communicate with other devices at
    baud rates ranging from 300 to 4608000. The UART data width is always 8 bit, but
    the number of stop bits and the parity configuration can be selected by the user.
    The UART interface also supports flow control, and can perform CPU based (using
    a register interface) or DMA based data transfers.

    The UART API provides functions to power up the UART interface, configure the
    interface parameters and to initiate data transfers.

    **\see\n
     *\see CyU3PUartInit
     *\see CyU3PUartSetConfig

    \subsection I2cIntf I2C interface
    \brief the I2C API allows users to configure the I2C interface on the FX3 device
    and to perform I2C data transfers.

    <B>Description</B>\n
    The I2C interface on the FX3 device is a master-only implementation that can work
    with multiple slave devices in a single-master or multi-master configuration. The
    interface can be configured to function at all of the standard I2C frequencies.

    The I2C API in the FX3 SDK allows the user to power up the I2C interface, configure
    the I2C interface and perform data transfers from/to a variety of slave devices.

    **\see\n
     *\see CyU3PI2cInit
     *\see CyU3PI2cSetConfig

    \subsection SpiIntf SPI interface
    \brief The SPI API allows users to configure the SPI interface and perform transfers
    from/to one or more SPI slave devices.

    <B>Description</B>\n
    The SPI interface on the FX3 device is a master-only implementation that can work
    with one or more slave devices at a variety of frequencies. The interface provides
    a single SPI Slave Select (SSN) pin that is directly controlled by the SPI block. FX3
    GPIOs need to be used as the SSN pin when talking to additional slave devices.

    The SPI API allows the user to configure the SPI interface parameters and to perform
    data transfers. Both CPU based and DMA based data transfers are supported; and read
    and write transfers can be performed individually or together.

    **\see\n
     *\see CyU3PSpiInit
     *\see CyU3PSpiSetConfig

    \subsection I2sIntf I2S interface
    \brief The I2S API in the FX3 SDK allows users to configure the I2S interface and perform
    data transfers out on one or two audio channels.

    <B>Description</B>\n
    The Inter-IC-Sound (I2S) interface is a serial wire interface used for audio streams. The
    FX3 device provides an I2S master interface that can output stereo or mono audio streams
    at different bit rates.

    The I2S API allows users to configure the I2S interface parameters, and to perform write
    transfers to the external I2S device. Even though CPU based transfers are supported, it
    is expected that DMA transfers will typically be used when talking to I2S devices.

    **\see\n
     *\see CyU3PI2sInit
     *\see CyU3PI2sSetConfig

    \subsection GpioIntf General Purpose IO (GPIO) Support
    \brief Almost all pins on the FX3 device which are unused for other functions can
    be used as GPIOs. The GPIO API supports configuration of the GPIO functionality.

    <B>Description</B>\n
    Most of the IOs on the FX3 device can be multiplexed to perform one of many different
    functions. Any of these IOs that are otherwise unused can be configured as a General
    Purpose IO (GPIO) pin.

    The FX3 device supports two classes of GPIO pins:
      -# Simple GPIO: The simple GPIOs are standard IO pins that also support a
         programmable interrupt function.
      -# Complex GPIO: A few of the GPIOs can be configured to perform more complex
         operations such as PWM output, input pulse width measurement etc. These
         GPIOs have an associated timer which can be used to measure elapsed time during
         CPU operations as well.

    FX3 also provides software controlled pull-up or pull-down resistors internally on
    all the digital I/O pins. These can be enabled/disabled individually for any of the
    GPIO pins.

    **\see\n
     *\see CyU3PGpioInit
     *\see CyU3PGpioSetSimpleConfig
     *\see CyU3PGpioSetComplexConfig

    \section GpifApi GPIF II Driver and API
    \brief The GPIF II Driver and API allows users to configure the GPIF II interface
    functionality and to control data transfers across the GPIF II interface.
    
    <B>Description</B>\n
    The FX3 device features a GPIF II interface that allows it to be connected
    to other controllers, ASICs or FPGAs. The programmability of the GPIF II interface
    allows it to meet the connectivity and timing requirements of the peer device
    without any glue logic. FX3 can act as a master or as a slave device for transfers
    across the GPIF II interface.

    The GPIF II Designer utility in the FX3 SDK allows users to define the signal and
    timing parameters for the required interface, and generates the corresponding
    configuration values in the form of a C header file. This can then be integrated
    into the FX3 firmware application using the GPIF API.

    The GPIF API provides functions to initialize the interface and perform control
    operations.

    \subsection gpifConf GPIF Configuration
    \brief The GPIF configuration for a specific protocol is typically generated
    in the form of a set of data structures by the GPIF II Designer tool and
    programmed using a set of GPIF APIs that take these data structures as parameters.

    <B>Description</B>\n
    The GPIF block is configured by writing to a set of device registers and
    configuration memories. The GPIF II Designer utility can be used to generate
    the configuration data corresponding to the communication protocol to be
    implemented. This tool generates the GPIF configuration information in the form
    of a C header file that defines a set of data structures. The FX3 SDK and the
    GPIF II Designer installation also include a set of header files that have the
    configuration data for a number of commonly used protocols.

    The CyU3PGpifLoad() API can be used by the firmware application to load
    the configuration generated by the GPIF II Designer tool. This API in turn
    internally uses the CyU3PGpifWaveformLoad(), CyU3PGpifInitTransFunctions()
    and CyU3PGpifConfigure() APIs to complete the configuration. These APIs can
    be used directly as a replacement to the CyU3PGpifLoad() call with the constraint
    that the CyU3PGpifConfigure() call should be made after the CyU3PGpifWaveformLoad()
    and CyU3PGpifInitTransFunctions() calls.

    **\see\n
     *\see CyU3PGpifLoad
     *\see CyU3PGpifWaveformLoad
     *\see CyU3PGpifInitTransFunctions
     *\see CyU3PGpifConfigure

    \subsection gpifResource GPIF-II Resources
    \brief The GPIF block implements multiple counter and comparator elements that can be
    initialized and controlled using the GPIF API.

    <B>Description</B>\n
    The GPIF II hardware block also implements a set of hardware resources that
    supplement the GPIF state machine operation. These resources include three counters
    and comparators that can be used to compare data, address and control signal
    values against preset threshold values.

    The counter resources include a 16 bit control counter, a 32 bit address counter and
    a 32 bit data counter. These counters are initialized through the CyU3PGpifLoad() API
    as part of the configuration load or at other points through the CyU3PGpifInitCtrlCounter(),
    CyU3PGpifInitAddrCounter() and CyU3PGpifInitDataCounter() APIs. The counters can be reset
    explicitly through these APIs or by the GPIF state machine. The counters can only be
    updated (increment/decrement) through the GPIF state machine. When the count value
    reaches the programmed limit, a GPIF callback function can be generated. The counter
    match condition can also be used as a transition trigger variable in the state machine.

    There are three comparators: a control comparator, an address comparator and a data
    comparator that can be used to continuously check the current values on the control,
    address and data signal groups against a user configured pattern. The patterns for
    comparison can be programmed initially through the CyU3PGpifLoad() API and at later
    points through the CyU3PGpifInitComparator() API. When any of the comparators detect
    a match, a GPIF callback function can be generated. The comparator match can also
    be used as a transition trigger variable in the state machine.

    **\see\n
     *\see CyU3PGpifInitComparator
     *\see CyU3PGpifInitCtrlCounter
     *\see CyU3PGpifInitDataCounter
     *\see CyU3PGpifInitAddrCounter

    \subsection gpifControl GPIF State Machine Control
    \brief A set of GPIF APIs are provided to start and control the operation of
    the GPIF state machine.

    <B>Description</B>\n
    The firmware application may require the GPIF state machine to be started and
    stopped multiple times during runtime.

    In most cases where the FX3 device is functioning as a slave device, there will
    be a single GPIF state machine and the application needs to start it as soon as
    the configuration has been loaded. The CyU3PGpifSMStart() API can be used to
    start the state machine operation in this case.

    If the application is using the FX3 device as a master on the GPIF interface, there
    may be multiple disjoint state machines that implement different parts of the
    communication protocol. The CyU3PGpifSMSwitch() API can be used to switch between
    different state machines in this case.

    There may be cases where the GPIF state machine operation is to be suspended and
    later resumed. The CyU3PGpifSMControl() API can be used to pause or resume state
    machine operation.

    If the state machine operation is to be stopped at some point and restarted from
    the reset state later, the CyU3PGpifDisable() API can be used with the forceReload
    parameter set to CyFalse. The CyU3PGpifSMStart() API can then be used to restart
    the state machine.

    If the active GPIF configuration is to be changed, the CyU3PGpifDisable() API can
    be used with the forceReload parameter set to CyTrue. The CyU3PGpifLoad() API can
    then be used to load a fresh configuration.

    **\see\n
     *\see CyU3PGpifSMStart
     *\see CyU3PGpifSMSwitch
     *\see CyU3PGpifDisable

    \section StorApi Storage Driver and API
    \brief The Storage driver and API for FX3S supports initialization of and data
    transfers from/to SD/MMC/SDIO peripherals.

    <B>Description</B>\n
    The FX3S device has two storage ports which can be connected to SD cards, eMMC
    devices or SDIO cards. The device and the storage driver support SD cards upto
    version 3.0, eMMC devices upto version 4.41 and SDIO cards upto version 3.0.

    The storage driver identifies the type of storage device connected on each of the
    enabled storage ports, and initializes it with the appropriate command sequence.
    The storage API provides functions to query the storage device properties like
    device type, memory capacity, number of SDIO functions etc. APIs are provided
    to perform read/write transfers to SD/MMC memory devices as well as to SDIO
    cards.

    The storage API also provides functions to send individual commands to the storage
    device and obtain the corresponding responses. The device initialization will be
    performed by the storage driver in this case as well, and the command mode can
    be used only after device initialization.

    **\see\n
     *\see CyU3PSibStart
     *\see CyU3PSibQueryDevice
     *\see CyU3PSibReadWriteRequest
     *\see CyU3PSibVendorAccess
     *\see CyU3PSdioByteReadWrite
     *\see CyU3PSdioExtendedReadWrite

    \section MipiApi MIPI-CSI2 and Fixed Function GPIF Interface for CX3
    \brief The CX3 device provides the ability to interface with and perform uncompressed 
    video transfers from Image Sensors implementing the MIPI CSI-2 interface.

    <B>Description</B>\n
    The CX3 device provides a MIPI-CSI2 interface block which can be interfaced with 
    Image Sensors implementing the MIPI-CSI2 interface and is capable of performing uncompressed 
    high-definition video transfers from such sensors via a fixed-function GPIF II interface. 
   
    This SDK release provides API support for configuring and utilizing the MIPI-CSI2 interface 
    block and configuration APIs for setting the GPIF II interface width for the fixed function
    GPIF II interface on the CX3 part.

    Additionally, example projects which demonstrate the usage of CX3 MIPI-CSI2 APIs are provided.
    The example projects implement high-definition (1920x1080 pixels, 16/24Bits per pixel) uncompressed 
    video transfers at upto 30 frames per second from an Aptina AS0260 image sensor over USB 3.0.
    An example project demonstrating 5Megapixel (2592x1944 pixels, 16 bits per pisxel) uncompressed
    video transfers at upto 15 frames per second from an Omnivision OV5640 image sensor over USB 3.0 
    is also provided.

 
    **\see\n
     *\see CyU3PMipicsiInit
     *\see CyU3PMipicsiDeInit
     *\see CyU3PMipicsiSetIntfParams
     *\see CyU3PMipicsiQueryIntfParams
     *\see CyU3PMipicsiReset
     *\see CyU3PMipicsiSleep
     *\see CyU3PMipicsiWakeup
     *\see CyU3PMipicsiGetErrors
     *\see CyU3PMipicsiGpifLoad

    \section MemAlloc Memory Allocation
    \brief The FX3 firmware makes use of dynamic memory allocation to obtain memory for the
    RTOS objects (thread stacks, message queues etc.) and for the DMA buffers required during
    channel creation. A set of functions are made available to perform this dynamic memory
    allocation.

    \subsection MemDrvHeap Driver Heap
    \brief This is a memory allocator that is used for allocating memory required for the
    FX3 driver threads. This allocator can also be used to allocate memory required by RTOS
    objects created in the user application code.

    <B>Description</B>\n
    The drivers for various blocks in the FX3 device make use of RTOS threads for execution,
    so that the driver operation need not be scheduled into the application code flow. The SDK
    obtains memory for the thread specific stacks and other OS objects (message queues) using
    the CyU3PMemAlloc function. This function is not provided within the SDK libraries, but is
    provided in source form in the cyfxtx.c file embedded in all of the firmware examples.

    The default implementation of this function (in cyfxtx.c) makes use of the ThreadX byte pool
    services. If required, users can re-implement the following functions to use a different
    memory allocation scheme.

    Buffers allocated through these functions do not have any specific alignment requirements.
    As with all memory allocators, the buffers will be aligned to 4-byte (DWORD) boundaries
    in terms of address and size.

    **\see\n
     *\see CyU3PMemInit
     *\see CyU3PMemAlloc
     *\see CyU3PMemFree
     *\see CyU3PFreeHeaps

    \subsection MemBufHeap Buffer Heap
    \brief This is a memory allocator that is used to allocate memory required for the buffers
    used in DMA transfers through the FX3 device.

    <B>Description</B>\n
    This is a memory allocator used to get memory required for the buffers used in DMA transfers
    through the FX3 device. This allocator is also provided in source form as part of the
    cyfxtx.c file, and can be modified if required by the application.

    The DMA APIs for FX3 internally make use of these functions during DMA channel creation and
    destruction. These functions can also be called from the user application code itself.

    The CyU3PDmaBufferInit function responsible for setting up the allocator, and the memory is
    obtained and freed using the CyU3PDmaBufferAlloc and CyU3PDmaBufferFree functions. It is required
    that DMA buffers on FX3 be aligned with cache lines (32 bytes) in terms of address and size.
    The default implementation provided in the cyfxtx.c satisfies the requirement. This is a
    custom implementation which imposes a fixed sized memory overhead instead of a per-buffer
    allocation overhead.

    **\see\n
     *\see CyU3PDmaBufferInit
     *\see CyU3PDmaBufferDeInit
     *\see CyU3PDmaBufferAlloc
     *\see CyU3PDmaBufferFree

    \subsection LeakCheck Memory Leak Checking
    \brief The driver and buffer heap implementations provided by Cypress have provision for
    run-time memory leak detection.

    <B>Description</B>\n
    The driver and buffer heap implementations provided by Cypress have provision for run-time
    memory leak detection. The memory leak detection is enabled using the CyU3PMemEnableChecks
    and CyU3PBufEnableChecks functions respectively.

    If the memory leak detection is enabled, the allocator adds a 32 byte header and a 4 byte
    footer around each memory block that is allocated. Since the checks change the behavior of
    the allocator, it is required that the enable function be called before the allocator is
    set up using the CyU3PMemInit or CyU3PDmaBufferInit function.

    The MemBlockInfo structure defined in cyu3os.h defines the format of the 32 byte header
    that preceded each memory block. This structure is used to maintain a doubly linked list of
    all allocated memory blocks. Memory leaks can be detected by calling the CyU3PMemGetActiveList
    or CyU3PBufGetActiveList function. These functions will return a pointer to the head of
    the list of active memory blocks. The list can be traversed using the prev_blk pointer to
    identify all active (unfreed) memory blocks and their allocation ids and sizes.

    Please note that the active list will also contain memory blocks that are allocated
    internally by the drivers in the SDK. These blocks can be identified by getting the active
    list before allocating any memory from the application code; and then stopping the list
    traversal when this node has been reached.

    **\see\n
     *\see CyU3PMemEnableChecks
     *\see CyU3PMemGetActiveList
     *\see CyU3PBufEnableChecks
     *\see CyU3PBufGetActiveList

    \subsection CorruptCheck Memory Corruption Detection
    \brief The driver and buffer heap implementations from Cypress support detection of memory
    corruption around the buffers allocated.
   
    <B>Description</B>\n
    As the FX3 system RAM is being shared between DMA buffers, user code and data; it
    is possible that small programming errors cause failures due to memory corruption. The
    driver and buffer memory allocators support detection of memory corruption around the
    allocated buffers.

    The corruption detection is performed by placing signatures around the allocated memory
    blocks and checking for valid signatures on user request. As with the memory leak
    detection, the memory corruption detection is also enabled using the CyU3PMemEnableChecks
    and CyU3PBufEnableChecks functions.

    The actual corruption check is done when any memory is being freed or on explicit user
    request. When memory is being freed using CyU3PMemFree or CyU3PDmaBufferFree, the signatures
    around the buffer being freed are checked. The signatures around all active (un-freed)
    memory blocks can be checked by calling the CyU3PMemCorruptionCheck or CyU3PBufCorruptionCheck
    functions.

    The user provided bad memory callback function is called if any corrupted signature is found
    during the check. Please note the check is stopped at the first instance of corruption, as it
    is likely that the linked list pointers in the MemBlockInfo header have also been corrupted
    when the signature is bad.

    **\see\n
     *\see CyU3PMemEnableChecks
     *\see CyU3PMemCorruptionCheck
     *\see CyU3PBufEnableChecks
     *\see CyU3PBufCorruptionCheck
 */

#endif /* _INCLUDED_CYFXAPIDESC_H_ */

/*[]*/

