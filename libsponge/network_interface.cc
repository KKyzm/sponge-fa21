#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "parser.hh"

#include <iostream>
#include <iterator>
#include <utility>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    EthernetFrame ethernet_frame;
    ethernet_frame.header().src = _ethernet_address;
    ethernet_frame.header().type = EthernetHeader::TYPE_IPv4;
    ethernet_frame.payload() = dgram.serialize();

    // check the arp_table
    bool arp_hit = false;
    if (_arp_table.count(next_hop_ip) != 0) {
        ethernet_frame.header().dst = _arp_table[next_hop_ip]._mac;
        // _arp_table[next_hop_ip]._ttl = _arp_item_expire_limit;
        arp_hit = true;
    }

    // if arp_table hits, send the ethernet_frame directly,
    // else send arp_request or join the waiting list.
    if (arp_hit)
        _frames_out.push(ethernet_frame);
    else {
        if (_frames_waiting_arp_reply.count(next_hop_ip) == 0) {
            send_request_arp(next_hop_ip);
            _frames_waiting_arp_reply.insert(make_pair(next_hop_ip, ethernet_frame));
        } else
            _frames_waiting_sending.insert(make_pair(next_hop_ip, ethernet_frame));
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    // only receive frame that dst_mac_address is broadcast or _ethernet_address
    if (frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST)
        return nullopt;

    // 	   IPV4 frame -> return datagram
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram dgram;
        if (dgram.parse(frame.payload()) == ParseResult::NoError)
            return dgram;
        else
            return nullopt;

        //! ARP frame  -> update arp_table
    } else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage arpgram;
        EthernetAddress src_mac;
        uint32_t src_ip;

        if (arpgram.parse(frame.payload()) == ParseResult::NoError) {
            src_mac = arpgram.sender_ethernet_address;
            src_ip = arpgram.sender_ip_address;
            if (_arp_table.count(src_ip) == 0)
                _arp_table.insert(make_pair(src_ip, arp_item(src_mac, _arp_item_expire_limit)));
            else
                _arp_table[src_ip]._ttl = _arp_item_expire_limit;
        } else
            return nullopt;

        if (arpgram.target_ip_address != _ip_address.ipv4_numeric())
            return nullopt;

        // reply an arp_request_gram
        if (arpgram.opcode == ARPMessage::OPCODE_REQUEST) {
            ARPMessage reply_arp;
            reply_arp.opcode = ARPMessage::OPCODE_REPLY;
            reply_arp.sender_ethernet_address = _ethernet_address;
            reply_arp.sender_ip_address = _ip_address.ipv4_numeric();
            reply_arp.target_ethernet_address = arpgram.sender_ethernet_address;
            reply_arp.target_ip_address = arpgram.sender_ip_address;

            EthernetFrame reply_frame;
            reply_frame.header().src = _ethernet_address;
            reply_frame.header().dst = arpgram.sender_ethernet_address;
            reply_frame.header().type = EthernetHeader::TYPE_ARP;
            reply_frame.payload() = reply_arp.serialize();
            _frames_out.push(reply_frame);

            // react to arp_reply_gram's arraival
        } else if (arpgram.opcode == ARPMessage::OPCODE_REPLY) {
            if (_frames_waiting_arp_reply.count(src_ip) != 0) {
                EthernetFrame frame_to_send = _frames_waiting_arp_reply[src_ip];
                _frames_waiting_arp_reply.erase(src_ip);
                frame_to_send.header().dst = src_mac;
                _frames_out.push(frame_to_send);

                // check that if some frame in waiting_list can be sent now
                if (_frames_waiting_sending.count(src_ip) != 0) {
                    EthernetFrame ethernet_frame = _frames_waiting_sending[src_ip];
                    _frames_waiting_sending.erase(src_ip);
                    ethernet_frame.header().dst = frame.header().src;
                    _frames_out.push(ethernet_frame);
                } else if (!_frames_waiting_sending.empty()) {
                    auto iter_to_begin = _frames_waiting_sending.begin();
                    EthernetFrame ethernet_frame = iter_to_begin->second;
                    send_request_arp(iter_to_begin->first);
                    _frames_waiting_arp_reply.insert(make_pair(iter_to_begin->first, iter_to_begin->second));
                    _frames_waiting_sending.erase(iter_to_begin);
                }
            }
        }
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _timer += ms_since_last_tick;
    if (_timer >= _retrans_limiter) {
        if (!_frames_waiting_arp_reply.empty())
            send_request_arp(_frames_waiting_arp_reply.begin()->first);
        _timer = 0;
    }

    auto iter = _arp_table.begin();
    while (iter != _arp_table.end()) {
        if (iter->second._ttl > ms_since_last_tick) {
            iter->second._ttl -= ms_since_last_tick;
            iter++;
        } else
            iter = _arp_table.erase(iter);
    }
}

void NetworkInterface::send_request_arp(uint32_t dst_ip) {
    ARPMessage request_arp;
    request_arp.opcode = ARPMessage::OPCODE_REQUEST;
    request_arp.sender_ethernet_address = _ethernet_address;
    request_arp.sender_ip_address = _ip_address.ipv4_numeric();
    request_arp.target_ethernet_address = {0, 0, 0, 0, 0, 0};
    request_arp.target_ip_address = dst_ip;

    EthernetFrame request_frame;
    request_frame.header().src = _ethernet_address;
    request_frame.header().dst = ETHERNET_BROADCAST;
    request_frame.header().type = EthernetHeader::TYPE_ARP;
    request_frame.payload() = request_arp.serialize();
    _frames_out.push(request_frame);
}
