#!/usr/bin/env python3

"""     
 Copyright (c) 2019 Cisco Systems, Inc. All rights reserved.
 License at https://github.com/cisco/mercury/blob/master/LICENSE
"""

import os
import sys
import copy
import dpkt
import pcap
import time
import socket
import datetime
import optparse
import ujson as json
from binascii import hexlify, unhexlify
from collections import OrderedDict

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from protocols.tls import TLS


class Fingerprinter:

    def __init__(self, database, output=None, analyze=False, num_procs=0, human_readable=False, experimental=False):
        if database == '../resources/fingerprint_db.json.gz':
            database = os.path.dirname(os.path.abspath(__file__)) + '/../resources/fingerprint_db.json.gz'
        self.analyze = analyze
        self.num_procs = num_procs
        self.human_readable = human_readable

        if output == None:
            self.out_file_pointer = None
        elif output == sys.stdout:
            self.out_file_pointer = sys.stdout
        else:
            self.out_file_pointer = open(output, 'w')

        # register parsers
        self.app_parsers = [('tls', TLS(database))]
        self.tcp_parsers = []
        self.ip_parsers  = []
        if experimental == True:
            from protocols.tcp import TCP
            from protocols.http import HTTP
            from protocols.tls_server import TLS_Server

            self.app_parsers.extend([('tls_server', TLS_Server()), ('http', HTTP())])
            self.tcp_parsers.extend([('tcp', TCP())])
            self.ip_parsers.extend([])
        self.all_parsers = self.app_parsers + self.tcp_parsers + self.ip_parsers


    def process(self, input_files):
        if len(input_files) == 0:
            print('error: need a pcap/interface')
            return 1

        self.data_ = []
        for input_file in input_files:
            if os.path.isfile(input_file):
                f = open(input_file,'rb')
                file_magic = str(hexlify(f.read(4)), 'utf-8')
                f.seek(0)
                if file_magic == '0d0a0a0d' or file_magic == '0a0d0d0a':
                    packets = dpkt.pcapng.Reader(f)
                else:
                    packets = dpkt.pcap.Reader(f)
                capture_type = 'offline'
            else:
                packets = pcap.pcap(input_file, timeout_ms=1000)
                capture_type = 'online'

            while True:
                pkts = packets.readpkts()

                for ts, buf in pkts:
                    try:
                        eth = dpkt.ethernet.Ethernet(buf)
                    except:
                        break # no data error?
                    ip = eth.data

                    if (type(ip) != dpkt.ip.IP and type(ip) != dpkt.ip6.IP6) or type(ip.data) != dpkt.tcp.TCP:
                        continue

                    tcp_data = ip.data
                    app_data = tcp_data.data

                    if type(ip) == dpkt.ip.IP:
                        add_fam = socket.AF_INET
                    else:
                        add_fam = socket.AF_INET6

                    results_ = []
                    for _,parser_ in self.ip_parsers:
                        results_.append(parser_.fingerprint(ip))
                    for _,parser_ in self.tcp_parsers:
                        results_.append(parser_.fingerprint(tcp_data))
                    for _,parser_ in self.app_parsers:
                        results_.append(parser_.fingerprint(app_data))

                    for protocol_type, fp_str_, approx_fp_str_, context_ in results_:
                        if fp_str_ != None:
                            flow = OrderedDict({})
                            flow['src_ip']       = socket.inet_ntop(add_fam,ip.src)
                            flow['dst_ip']       = socket.inet_ntop(add_fam,ip.dst)
                            flow['src_port']     = tcp_data.sport
                            flow['dst_port']     = tcp_data.dport
                            flow['protocol']     = ip.p
                            flow['event_start']  = str(datetime.datetime.utcfromtimestamp(ts))
                            if context_ != None:
                                flow['context'] = OrderedDict({})
                                for x_ in context_:
                                    flow['context'][x_['name']]  = x_['data']
                            flow['fingerprints'] = OrderedDict({})
                            if approx_fp_str_ != None:
                                flow['fingerprints'][protocol_type] = fp_str_
                                flow['fingerprints'][protocol_type+'_approx'] = approx_fp_str_
                            else:
                                flow['fingerprints'][protocol_type] = fp_str_
                            if self.human_readable:
                                for parse_type, parser_ in self.all_parsers:
                                    if parse_type == protocol_type:
                                        desc_ = parser_.get_human_readable(fp_str_)
                                        if desc_ != None:
                                            flow[protocol_type] = desc_
                            if self.analyze:
                                if approx_fp_str_ != None:
                                    fp_str_ = approx_fp_str_
                                for parse_type, parser_ in self.all_parsers:
                                    if parse_type == protocol_type:
                                        proc_info = parser_.proc_identify(fp_str_, context_, flow['dst_ip'],
                                                                          flow['dst_port'], self.num_procs)
                                        if proc_info != None:
                                            if 'analysis' not in flow:
                                                flow['analysis'] = OrderedDict({})
                                            flow['analysis'] = proc_info
                                            break

                            self.write_record(flow)

                if capture_type == 'offline':
                    break


    def lookup_fingerprint_string(self, fp_str):
        for protocol_type, parser_ in self.all_parsers:
            fp_str_ = bytes(fp_str, 'utf-8')
            if fp_str_ in parser_.fp_db:
                fp_ = parser_.fp_db[fp_str_]
                self.write_record(fp_)


    def write_record(self, flow_repr):
        if flow_repr != None:
            self.out_file_pointer.write('%s\n' % json.dumps(flow_repr))
            self.out_file_pointer.flush()



def main():
    start = time.time()

    parser = optparse.OptionParser()

    parser.add_option('-c','--capture',action='store',dest='capture_interface',
                      help='live packet capture',default=None)
    parser.add_option('-r','--read',action='store',dest='pcap_file',
                      help='read packets from file',default=None)
    parser.add_option('-d','--fp_db',action='store',dest='fp_db',
                      help='location of fingerprint database',default='../resources/fingerprint_db.json.gz')
    parser.add_option('-f','--fingerprint',action='store',dest='output',
                      help='write fingerprints to file',default=sys.stdout)
    parser.add_option('-l','--lookup',action='store',dest='lookup',
                      help='lookup fingerprint string <fp_str>',default=None)
    parser.add_option('-a','--analysis',action='store_true',dest='analyze',
                      help='perform process identification',default=False)
    parser.add_option('-n','--num-procs',action='store',dest='num_procs',type='int',
                      help='return the top-n most probable processes',default=0)
    parser.add_option('-w','--human-readable',action='store_true',dest='human_readable',
                      help='return human readable fingerprint information',default=False)
    parser.add_option('-e','--experimental',action='store_true',dest='experimental',
                      help='turns on all experimental features',default=False)

    options, args = parser.parse_args()

    input_files = []
    if options.pcap_file != None:
        input_files.append(options.pcap_file)
    for x in args:
        if '.pcap' in x:
            input_files.append(x)
    if options.capture_interface != None:
        input_files.append(options.capture_interface)

    fingerprinter = Fingerprinter(options.fp_db, options.output, options.analyze,
                                  options.num_procs, options.human_readable, options.experimental)

    if options.lookup != None:
        fingerprinter.lookup_fingerprint_string(options.lookup)
    elif len(input_files) > 0:
        fingerprinter.process(input_files)
    else:
        print('error: need a pcap/interface or fingerprint string for lookup')



if __name__ == '__main__':
    sys.exit(main())
