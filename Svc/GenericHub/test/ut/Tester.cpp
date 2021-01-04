// ======================================================================
// \title  GenericHub.hpp
// \author mstarch
// \brief  cpp file for GenericHub test harness implementation class
//
// \copyright
// Copyright 2009-2015, by the California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
//
// ======================================================================

#include "Tester.hpp"
#include <STest/Pick/Pick.hpp>

#define INSTANCE 0
#define MAX_HISTORY_SIZE 100000

namespace Svc {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

Tester ::Tester(void)
    : GenericHubGTestBase("Tester", MAX_HISTORY_SIZE),
      componentIn("GenericHubIn"),
      componentOut("GenericHubOut"),
      m_buffer(m_data_store, DATA_SIZE),
      m_comm_in(0),
      m_buffer_in(0),
      m_comm_out(0),
      m_buffer_out(0),
      m_current_port(0) {
    this->initComponents();
    this->connectPorts();
}

Tester ::~Tester(void) {}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void Tester ::test_in_out(void) {
    for (U32 i = 0; i < 10; i++) {
        send_random_comm(i);
    }
}

void Tester ::test_buffer_io(void) {
    for (U32 i = 0; i < 10; i++) {
        send_random_buffer(i);
    }
}

void Tester ::test_random_io(void) {
    for (U32 i = 0; i < 100000; i++) {
        U32 port = STest::Pick::lowerUpper(0, 9);
        U32 choice = STest::Pick::lowerUpper(0, 1);
        if (choice) {
            send_random_comm(port);
        } else {
            send_random_buffer(port);
        }
    }
}
// Helpers

void Tester ::send_random_comm(U32 port) {
    U32 random_size = STest::Pick::lowerUpper(0, FW_COM_BUFFER_MAX_SIZE);
    m_comm.resetSer();
    for (U32 i = 0; i < random_size; i++) {
        m_comm.serialize(static_cast<U8>(STest::Pick::any()));
    }
    m_current_port = port;
    invoke_to_portIn(m_current_port, m_comm);
    // Ensure that the data out was called, and that the portOut unwrapped properly
    ASSERT_from_dataOut_SIZE(m_comm_in + m_buffer_out + 1);
    ASSERT_EQ(m_comm_in + 1, m_comm_out);
    m_comm_in++;
}

void Tester ::send_random_buffer(U32 port) {
    U32 random_size = STest::Pick::lowerUpper(0, DATA_SIZE);

    for (U32 i = 0; i < random_size; i++) {
        reinterpret_cast<U8*>(m_buffer.getData())[i] = static_cast<U8>(STest::Pick::any());
    }
    m_current_port = port;
    invoke_to_buffersIn(m_current_port, m_buffer);
    // Ensure that the data out was called, and that the portOut unwrapped properly
    ASSERT_from_dataOut_SIZE(m_buffer_in + m_comm_out + 1);
    ASSERT_EQ(m_buffer_in + 1, m_buffer_out);
    m_buffer_in++;
}

// ----------------------------------------------------------------------
// Handlers for typed from ports
// ----------------------------------------------------------------------

Drv::SendStatus Tester ::from_dataOut_handler(const NATIVE_INT_TYPE portNum, Fw::Buffer& fwBuffer) {
    this->pushFromPortEntry_dataOut(fwBuffer);
    invoke_to_dataIn(0, fwBuffer, Drv::RECV_OK);
    return Drv::SEND_OK;
}

// ----------------------------------------------------------------------
// Handlers for serial from ports
// ----------------------------------------------------------------------

void Tester ::from_buffersOut_handler(const NATIVE_INT_TYPE portNum, Fw::Buffer& fwBuffer) {
    m_buffer_out++;
    // Assert the buffer came through exactly on the right port
    ASSERT_EQ(portNum, m_current_port);
    ASSERT_EQ(fwBuffer.getSize(), m_buffer.getSize());
    for (U32 i = 0; i < fwBuffer.getSize(); i++) {
        U8 byte1 = reinterpret_cast<U8*>(fwBuffer.getData())[i];
        U8 byte2 = reinterpret_cast<U8*>(m_buffer.getData())[i];
        ASSERT_EQ(byte1, byte2);
    }
}

void Tester ::from_portOut_handler(NATIVE_INT_TYPE portNum,        /*!< The port number*/
                                   Fw::SerializeBufferBase& Buffer /*!< The serialization buffer*/
) {
    m_comm_out++;
    // Assert the buffer came through exactly on the right port
    ASSERT_EQ(portNum, m_current_port);
    ASSERT_EQ(Buffer.getBuffLength(), m_comm.getBuffLength());
    for (U32 i = 0; i < Buffer.getBuffLength(); i++) {
        ASSERT_EQ(Buffer.getBuffAddr()[i], m_comm.getBuffAddr()[i]);
    }
    ASSERT_from_buffersOut_SIZE(0);
}

Fw::Buffer Tester ::from_bufferAllocate_handler(const NATIVE_INT_TYPE portNum, const U32 size) {
    // Ensure the buffer has not be allocated
    EXPECT_EQ(m_allocate.getData(), nullptr) << "Buffer not deallocated before reallocation";
    EXPECT_LE(size, DATA_SIZE) << "Allocation size too large of UT";
    m_allocate.set(m_data_for_allocation, size);
    return m_allocate;
}

void Tester ::from_bufferDeallocate_handler(const NATIVE_INT_TYPE portNum, Fw::Buffer& fwBuffer) {
    EXPECT_EQ(fwBuffer.getData(), m_data_for_allocation) << "Invalid data pointer return to deallocation";
    m_allocate.setData(nullptr);
}

// ----------------------------------------------------------------------
// Helper methods
// ----------------------------------------------------------------------

void Tester ::connectPorts(void) {
    // buffersIn
    for (NATIVE_INT_TYPE i = 0; i < 10; ++i) {
        this->connect_to_buffersIn(i, this->componentIn.get_buffersIn_InputPort(i));
    }

    // dataIn
    this->connect_to_dataIn(0, this->componentOut.get_dataIn_InputPort(0));

    // buffersOut
    for (NATIVE_INT_TYPE i = 0; i < 10; ++i) {
        this->componentOut.set_buffersOut_OutputPort(i, this->get_from_buffersOut(i));
    }

    // dataOut
    this->componentIn.set_dataOut_OutputPort(0, this->get_from_dataOut(0));

    // ----------------------------------------------------------------------
    // Connect serial output ports
    // ----------------------------------------------------------------------
    for (NATIVE_INT_TYPE i = 0; i < 10; ++i) {
        this->componentOut.set_portOut_OutputPort(i, this->get_from_portOut(i));
    }

    // ----------------------------------------------------------------------
    // Connect serial input ports
    // ----------------------------------------------------------------------
    // portIn
    for (NATIVE_INT_TYPE i = 0; i < 10; ++i) {
        this->connect_to_portIn(i, this->componentIn.get_portIn_InputPort(i));
    }
}

void Tester ::initComponents(void) {
    this->init();
    this->componentIn.init(INSTANCE);
    this->componentOut.init(INSTANCE + 1);
}

}  // end namespace Svc
