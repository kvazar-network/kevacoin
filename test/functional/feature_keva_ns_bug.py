#!/usr/bin/env python3
# Copyright (c) 2016-2017 The Bitcoin Core developers
# Copyright (c) 2018-2019 The Kevacoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the Key-Value Store."""

from test_framework.blocktools import witness_script, send_to_witness
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
from test_framework.authproxy import JSONRPCException
from io import BytesIO
from string import Template
import decimal
import re
import json

class DecimalEncoder(json.JSONEncoder):
    def default(self, o):
        if isinstance(o, decimal.Decimal):
            return float(o)
        return super(DecimalEncoder, self).default(o)

class KevaTest(BitcoinTestFramework):

    def sync_generate(self, node, num):
        self.sync_all()
        node.generate(num)
        self.sync_all()

    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True

    def setup_network(self):
        self.setup_nodes()
        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[1], 0)
        connect_nodes(self.nodes[2], 0)
        self.sync_all()

    def dumpTx(self, tx):
        print(json.dumps(self.nodes[0].decoderawtransaction(self.nodes[0].getrawtransaction(tx)), indent=4, cls=DecimalEncoder))

    def run_test(self):
        self.nodes[0].generate(101)
        addr1 = self.nodes[1].getnewaddress()
        addr2 = self.nodes[2].getnewaddress()
        amount = Template('{"$addr1":1,"$addr2":1}').substitute(addr1=addr1, addr2=addr2)
        amountJson = json.loads(amount)
        self.nodes[0].sendmany("", amountJson)
        self.sync_generate(self.nodes[0], 1)
        ns1 = (self.nodes[1].keva_namespace('NS 1'))["namespaceId"]
        ns2 = (self.nodes[2].keva_namespace('NS 2'))["namespaceId"]
        self.sync_generate(self.nodes[0], 1)
        # Before nsfix is enabled, the namespace will be the same.
        # This is a historic bug.
        assert(ns1 == ns2)

if __name__ == '__main__':
    KevaTest().main()
