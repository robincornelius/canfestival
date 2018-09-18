/*
This file is part of CanFestival, a library implementing CanOpen Stack. 

CanFestival Copyright (C): Edouard TISSERANT and Francis DUPIN
CanFestival Win32 port Copyright (C) 2007 Leonid Tochinski, ChattenAssociates, Inc.

See COPYING file for copyrights details.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

// LAWICEL AB CANUSB adapter (http://www.can232.com/)
// driver for CanFestival-3 Win32 port

#include <sstream>
#include <iomanip>
#if 0  // change to 1 if you use boost
#include <boost/algorithm/string/case_conv.hpp>
#else
#include <algorithm>
#endif

// string::find_first_of
#include <iostream>       // std::cout
#include <string>         // std::string
#include <cstddef>        // std::size_t

extern "C" {
#include "can_driver.h"
}
class can_canusbd2xx_win32
   {
   public:
      class error
        {
        };
	  can_canusbd2xx_win32(s_BOARD *board);
	  ~can_canusbd2xx_win32();
      bool send(const Message *m);
      bool receive(Message *m);
   private:
      bool open_rs232(std::string port ="COM1", int baud_rate = 57600);
      bool close_rs232();
      bool get_can_data(const char* can_cmd_buf, long& bufsize, Message* m);
      bool set_can_data(const Message& m, std::string& can_cmd);
	  bool can_canusbd2xx_win32::doTX(std::string can_cmd);
   private:
      HANDLE m_port;
      HANDLE m_read_event;
      HANDLE m_write_event;
      std::string m_residual_buffer;
   };

can_canusbd2xx_win32::can_canusbd2xx_win32(s_BOARD *board) : m_port(INVALID_HANDLE_VALUE),
      m_read_event(0),
      m_write_event(0)
   {

	printf("HELLO WORLD\n");

   if (!open_rs232(board->busname))
      throw error();
   /*
    S0 Setup 10Kbit
	S1 Setup 20Kbit
	S2 Setup 50Kbit
	S3 Setup 100Kbit
	S4 Setup 125Kbit
	S5 Setup 250Kbit
	S6 Setup 500Kbit
	S7 Setup 800Kbit
	S8 Setup 1Mbit
   */

   doTX("C\r");

   if (!strcmp(board->baudrate, "10K"))
   {
	   doTX("S0\r");
   }

   if (!strcmp(board->baudrate, "20K"))
   {
	   doTX("S1\r");
   }

   if (!strcmp(board->baudrate, "50K"))
   {
	   doTX("S2\r");
   }

   if (!strcmp(board->baudrate, "100K"))
   {
	   doTX("S3\r");
   }

   if (!strcmp(board->baudrate, "125K"))
   {
	   doTX("S4\r");
   }

   if (!strcmp(board->baudrate, "250K"))
   {
	   doTX("S5\r");
   }

   if (!strcmp(board->baudrate, "500K"))
   {
	   doTX("S6\r");
   }

   if (!strcmp(board->baudrate, "800K"))
   {
	   doTX("S7\r");
   }

   if (!strcmp(board->baudrate, "1M"))
   {
	   doTX("S8\r");
   }

   doTX("O\r");


   }

can_canusbd2xx_win32::~can_canusbd2xx_win32()
   {
   close_rs232();
   }


bool can_canusbd2xx_win32::doTX(std::string can_cmd)
{

	OVERLAPPED overlapped;
	::memset(&overlapped, 0, sizeof overlapped);
	overlapped.hEvent = m_write_event;
	::ResetEvent(overlapped.hEvent);

	unsigned long bytes_written = 0;
	::WriteFile(m_port, can_cmd.c_str(), (unsigned long)can_cmd.length(), &bytes_written, &overlapped);
	// wait for write operation completion
	enum { WRITE_TIMEOUT = 1000 };
	::WaitForSingleObject(overlapped.hEvent, WRITE_TIMEOUT);
	// get number of bytes written
	::GetOverlappedResult(m_port, &overlapped, &bytes_written, FALSE);

	bool result = (bytes_written == can_cmd.length());

	return result;

}

bool can_canusbd2xx_win32::send(const Message *m)
   {
   if (m_port == INVALID_HANDLE_VALUE)
      return true;

   // build can_uvccm_win32 command string
   std::string can_cmd;
   set_can_data(*m, can_cmd);

   bool result = doTX(can_cmd);

   return false;
   }


bool can_canusbd2xx_win32::receive(Message *m)
   {
	
	m->cob_id = 0;
	m->len = 0;

	if (m_port == INVALID_HANDLE_VALUE)
	{
		return false;
	}

   long res_buffer_size = (long)m_residual_buffer.size();
   bool result = get_can_data(m_residual_buffer.c_str(), res_buffer_size, m);
   if (result)
   {
	  m_residual_buffer.erase(0, res_buffer_size);
      return true;
   }

   enum { READ_TIMEOUT = 500 };

   OVERLAPPED overlapped;
   ::memset(&overlapped, 0, sizeof overlapped);
   overlapped.hEvent = m_read_event;
   ::ResetEvent(overlapped.hEvent);
   unsigned long event_mask = 0;

   if (FALSE == ::WaitCommEvent(m_port, &event_mask, &overlapped) && ERROR_IO_PENDING == ::GetLastError())
      {
      if (WAIT_TIMEOUT == ::WaitForSingleObject(overlapped.hEvent, READ_TIMEOUT))
         return true;
      }

   // get number of bytes in the input que
   COMSTAT stat;
   ::memset(&stat, 0, sizeof stat);
   unsigned long errors = 0;
   ::ClearCommError(m_port, &errors, &stat);
   if (stat.cbInQue == 0)
	   return true;
   char buffer[3000];

   unsigned long bytes_to_read = min(stat.cbInQue, sizeof (buffer));

   unsigned long bytes_read = 0;
   ::ReadFile(m_port, buffer, bytes_to_read, &bytes_read, &overlapped);
   // wait for read operation completion
   ::WaitForSingleObject(overlapped.hEvent, READ_TIMEOUT);
   // get number of bytes read
   ::GetOverlappedResult(m_port, &overlapped, &bytes_read, FALSE);
   result = true;
   if (bytes_read > 0)
      {
      m_residual_buffer.append(buffer, bytes_read);
      res_buffer_size = (long)m_residual_buffer.size();
      result = get_can_data(m_residual_buffer.c_str(), res_buffer_size, m);
      if (result)
         m_residual_buffer.erase(0, res_buffer_size);
      }
  // return result;
   return true;
   }

bool can_canusbd2xx_win32::open_rs232(std::string port, int baud_rate)
   {
   if (m_port != INVALID_HANDLE_VALUE)
      return true;

   //std::ostringstream device_name;
   //device_name << "COM" << port;

   m_port = ::CreateFile(port.c_str(),
                         GENERIC_READ | GENERIC_WRITE,
                         0,   // exclusive access
                         NULL,   // no security
                         OPEN_EXISTING,
                         FILE_FLAG_OVERLAPPED,   // overlapped I/O
                         NULL); // null template

   // Check the returned handle for INVALID_HANDLE_VALUE and then set the buffer sizes.
   if (m_port == INVALID_HANDLE_VALUE)
      return false;

   //  SetCommMask(m_hCom,EV_RXCHAR|EV_TXEMPTY|EV_CTS|EV_DSR|EV_RLSD|EV_BREAK|EV_ERR|EV_RING); //
   ::SetCommMask(m_port, EV_RXFLAG);

   COMMTIMEOUTS timeouts;
   ::memset(&timeouts, 0, sizeof (timeouts));
   timeouts.ReadIntervalTimeout = -1;
   timeouts.ReadTotalTimeoutConstant = 0;
   timeouts.ReadTotalTimeoutMultiplier = 0;
   timeouts.WriteTotalTimeoutConstant = 5000;
   timeouts.WriteTotalTimeoutMultiplier = 0;
   SetCommTimeouts(m_port, &timeouts); //

   ::SetupComm(m_port, 1024, 512); // set buffer sizes

   // Port settings are specified in a Data Communication Block (DCB). The easiest way to initialize a DCB is to call GetCommState to fill in its default values, override the values that you want to change and then call SetCommState to set the values.
   DCB dcb;
   ::memset(&dcb, 0, sizeof (dcb));
   ::GetCommState(m_port, &dcb);
   dcb.BaudRate = baud_rate;
   dcb.ByteSize = 8;
   dcb.Parity = NOPARITY;
   dcb.StopBits = ONESTOPBIT;
   dcb.fAbortOnError = TRUE;
   dcb.EvtChar = 0x0A; // '\n' character
   ::SetCommState(m_port, &dcb);

   ::PurgeComm(m_port, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);

   m_read_event = ::CreateEvent(NULL, TRUE, FALSE, NULL);
   m_write_event = ::CreateEvent(NULL, TRUE, FALSE, NULL);

   return true;
   }

bool can_canusbd2xx_win32::close_rs232()
   {
   if (m_port != INVALID_HANDLE_VALUE)
      {
      ::PurgeComm(m_port, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
      ::CloseHandle(m_port);
      m_port = INVALID_HANDLE_VALUE;
      ::CloseHandle(m_read_event);
      m_read_event = 0;
      ::CloseHandle(m_write_event);
      m_write_event = 0;
      m_residual_buffer.clear();
      }
   return true;
   }

bool can_canusbd2xx_win32::get_can_data(const char* can_cmd_buf, long& bufsize, Message* m)
{
	if (bufsize < 5)
	{
		bufsize = 0;
		return false;
	}

	Message msg;
	::memset(&msg, 0, sizeof(msg));
	char colon = 0, type = 0, request = 0;

	std::string meh = can_cmd_buf;

	std::size_t found = meh.find_first_of('t');

	if (found == -1)
	{
		return true;
	}

	if (found > 0)
	{
		bufsize = found;
		return true;
	}

   std::istringstream buf(std::string(can_cmd_buf),32);

   std::string cob;
   std::string len;

   buf >> type >> std::setw(3) >> cob >> std::setw(1) >> len;

   buf.str();


   if (type != 't')
   {
	   bufsize = 0;
	   return false;
   }



   msg.cob_id = std::stoi(cob, 0, 16);
   msg.len = std::stoi(len, 0, 16);

   UNS8 pos;

   if (type == 't')
      {
      msg.rtr = 0;
	  for (pos = 0; pos < msg.len; pos++)
         {
         std::string data_byte_str;
         buf >> std::setw(2) >> data_byte_str;

         if (data_byte_str[0] == '\r')
            break;
         long byte_val = -1;
         std::istringstream(data_byte_str) >> std::hex >> byte_val;
         if (byte_val == -1)
            {
            bufsize = 0;
            return false;
            }
         msg.data[pos] = (UNS8)byte_val;
         }

      if (msg.len > 0)
         {
			
		 char semicolon = buf.get();
         if (semicolon != '\r')
            {
            bufsize = 0;
            return false;
            }
         }

      }
   else if (type == 'r')
      {
      msg.rtr = 1;
      buf >> msg.len;
      }
   else
      {
      bufsize = 0;
      return false;
      }

   bufsize = buf.tellg();

   *m = msg;
   return true;
   }

bool can_canusbd2xx_win32::set_can_data(const Message& m, std::string& can_cmd)
   {
   // build can_uvccm_win32 command string
   std::ostringstream can_cmd_str;

   //Normal or RTR, note lowercase for standard can frames, upper case t/r for extended COB IDS

   if (m.rtr == 1)
   {
	   can_cmd_str << 'r'; 
   }
   else
   {
	   can_cmd_str << 't';
   }

   //COB next

   can_cmd_str << std::hex << std::setfill('0') << std::setw(3) << m.cob_id;

   //LEN next

   can_cmd_str << std::hex << std::setfill('0') << std::setw(1) << (UNS16)m.len;

   //DATA

   for (int i = 0; i < m.len; ++i)
   {
	   can_cmd_str << std::hex << std::setfill('0') << std::setw(2) << (long)m.data[i];
   }

   //Terminate

   can_cmd_str << "\r";

   can_cmd = can_cmd_str.str();

   OutputDebugString(can_cmd.c_str());

   return false;
   }


//------------------------------------------------------------------------
extern "C"
   UNS8 __stdcall canReceive_driver(CAN_HANDLE fd0, Message *m)
   {
	   return (UNS8)(!(reinterpret_cast<can_canusbd2xx_win32*>(fd0)->receive(m)));
   }

extern "C"
   UNS8 __stdcall canSend_driver(CAN_HANDLE fd0, Message const *m)
   {
	   return (UNS8)reinterpret_cast<can_canusbd2xx_win32*>(fd0)->send(m);
   }

extern "C"
   CAN_HANDLE __stdcall canOpen_driver(s_BOARD *board)
   {
   try
      {
		  return (CAN_HANDLE) new can_canusbd2xx_win32(board);
      }
   catch (can_canusbd2xx_win32::error&)
      {
      return NULL;
      }
   }

extern "C"
   int __stdcall canClose_driver(CAN_HANDLE inst)
   {
	   delete reinterpret_cast<can_canusbd2xx_win32*>(inst);
   return 1;
   }

extern "C"
	UNS8 __stdcall canChangeBaudRate_driver( CAN_HANDLE fd, char* baud)
	{
	return 0;
	} 
