/*-
 * Copyright (c) 2012 Caoimhe Chaos <caoimhechaos@protonmail.com>,
 *                    Ancient Solutions. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions  of source code must retain  the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions  in   binary  form  must   reproduce  the  above
 *    copyright  notice, this  list  of conditions  and the  following
 *    disclaimer in the  documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS  SOFTWARE IS  PROVIDED BY  ANCIENT SOLUTIONS  AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO,  THE IMPLIED WARRANTIES OF  MERCHANTABILITY AND FITNESS
 * FOR A  PARTICULAR PURPOSE  ARE DISCLAIMED.  IN  NO EVENT  SHALL THE
 * FOUNDATION  OR CONTRIBUTORS  BE  LIABLE FOR  ANY DIRECT,  INDIRECT,
 * INCIDENTAL,   SPECIAL,    EXEMPLARY,   OR   CONSEQUENTIAL   DAMAGES
 * (INCLUDING, BUT NOT LIMITED  TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE,  DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT  LIABILITY,  OR  TORT  (INCLUDING NEGLIGENCE  OR  OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <mutex>
#include <siot/connection.h>
#include <siot/server.h>
#include <string>
#include <thread++/threadpool.h>
#include <toolbox/scopedptr.h>
#include "server.h"

namespace http
{
namespace server
{
using google::protobuf::Closure;
using google::protobuf::NewCallback;
using std::map;
using std::mutex;
using std::string;
using std::unique_lock;
using threadpp::ThreadPool;
using toolbox::siot::Connection;
using toolbox::siot::Server;

class TCPPeer : public Peer
{
public:
	TCPPeer(Protocol* proto, Connection* sock)
	: proto_(proto), sock_(sock)
	{
	}

	virtual Protocol* PeerProtocol()
	{
		return proto_;
	}

	virtual string PeerAddress()
	{
		return sock_->PeerAsText();
	}

	virtual Connection* PeerSocket()
	{
		return sock_;
	}

	virtual Server* Parent()
	{
		return sock_->GetServer();
	}

private:
	Protocol* proto_;
	Connection* sock_;
};

WebServer::WebServer()
: num_threads_(10), shutdown_(false)
{
}

WebServer::~WebServer()
{
}

void
WebServer::Handle(string pattern, Handler* handler)
{
	multiplexer_.Handle(pattern, handler);
}

void
WebServer::ListenAndServe(string addr, Protocol* protocol)
{
	Server srv(addr, 0, num_threads_);
	Serve(&srv, protocol);
}

void
WebServer::Serve(Server* srv, Protocol* proto)
{
	shutdown_ = false;
	unique_lock<mutex> lk(executor_lock_);

	if (executor_.IsNull())
		executor_.Reset(new ThreadPool(num_threads_));

	srv->SetConnectionCallback(new ProtocolServer(this, proto,
				&multiplexer_));
	servers_.push_back(srv);
	srv->Listen();
}

void
WebServer::SetExecutor(ThreadPool* executor)
{
	unique_lock<mutex> lk(executor_lock_);
	executor_.Reset(executor);
}

void
WebServer::SetNumThreads(uint32_t num_threads)
{
	num_threads_ = num_threads;
}

void
WebServer::Shutdown()
{
	for (Server* srv : servers_)
		srv->Shutdown();
}

ThreadPool*
WebServer::GetExecutor()
{
	return executor_.Get();
}

void
WebServer::ServeConnection(Peer* peer)
{
	Protocol* proto = peer->PeerProtocol();
	proto->DecodeConnection(executor_.Get(), &multiplexer_, peer);
}

ProtocolServer::ProtocolServer(WebServer* parent, Protocol* proto, ServeMux* mux)
: parent_(parent), proto_(proto), multiplexer_(mux)
{
}

ProtocolServer::~ProtocolServer()
{
}

void
ProtocolServer::DataReady(Connection* conn)
{
	TCPPeer peer(proto_, conn);
	proto_->DecodeConnection(parent_->GetExecutor(), multiplexer_, &peer);
}

void
ProtocolServer::ConnectionEstablished(Connection* conn)
{
}

void
ProtocolServer::ConnectionTerminated(Connection* conn)
{
}

void
ProtocolServer::Error(Connection* conn)
{
}

Protocol::~Protocol()
{
}
}  // namespace server
}  // namespace http
