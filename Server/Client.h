/*
 * Copyright (c) 2022, James Puleo <james@jame.xyz>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <AK/StdLibExtras.h>
#include <netinet/ip.h>

class Client
{
public:
    Client(sockaddr_in address) : m_address(move(address)) {}

    int client_challenge() const { return m_client_challenge; }
    int server_challenge() const { return m_server_challenge; }
    int client_packet_sequence() const { return m_client_packet_sequence; }
    int server_packet_sequence() const { return m_server_packet_sequence; }

    const sockaddr_in& address() const { return m_address; }
    void set_client_challenge(int value) { m_client_challenge = value; }
    void set_server_challenge(int value) { m_server_challenge = value; }
    void set_client_packet_sequence(int value) { m_client_packet_sequence = value; }
    void set_server_packet_sequence(int value) { m_server_packet_sequence = value; }

private:
    sockaddr_in m_address;
    int m_client_challenge{}; // This is the challenge this client gave us
    int m_server_challenge{}; // This is the challenge we gave the client
    // Sequences always start at 1
    int m_client_packet_sequence{1};
    int m_server_packet_sequence{1};
};