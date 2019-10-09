#!/usr/bin/env python3

"""     
 Copyright (c) 2019 Cisco Systems, Inc. All rights reserved.
 License at https://github.com/cisco/mercury/blob/master/LICENSE
"""

import os
import sys
import operator
import functools
import ujson as json
from sys import path
from math import exp, log
from binascii import hexlify, unhexlify

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(os.path.dirname(os.path.abspath(__file__))+'/../')
from protocol import Protocol
from utils.tls_utils import *
from utils.tls_constants import *
from utils.pmercury_utils import *
from utils.contextual_info import *
from utils.sequence_alignment import *

MAX_CACHED_RESULTS = 2**24


class TLS(Protocol):

    def __init__(self, fp_database=None):
        # cached data/results
        self.tls_params_db = {}
        self.MALWARE_DB = True

        # populate fingerprint databases
        self.fp_db = {}
        if fp_database == 'resources/fingerprint_db.json.gz':
            fp_database = find_resource_path(fp_database)
        if fp_database != None:
            self.load_database(fp_database)

        app_families_file = find_resource_path('resources/app_families.txt')
        self.app_families = {}
        for line in open(app_families_file, 'r'):
            tokens = line.strip().split(',')
            for i in range(1, len(tokens)):
                self.app_families[tokens[i]] = tokens[0]

        self.aligner = SequenceAlignment(f_similarity, 0.0)

        # TLS ClientHello pattern/RE
        self.tls_client_hello_mask  = b'\xff\xff\xfc\x00\x00\xff\x00\x00\x00\xff\xfc'
        self.tls_client_hello_value = b'\x16\x03\x00\x00\x00\x01\x00\x00\x00\x03\x00'


    def load_database(self, fp_database):
        for line in os.popen('zcat %s' % (fp_database)):
            fp_ = json.loads(line)
            fp_['str_repr'] = fp_['str_repr'].encode()

            for p_ in fp_['process_info']:
                p_['process'] = p_['process'].encode()
                if 'malware' not in p_:
                    self.MALWARE_DB = False

            self.fp_db[fp_['str_repr']] = fp_


    def fingerprint(self, data):
        if len(data) < 32:
            return None, None, None, []
        # check TLS version and record/handshake type
        for i in range(11):
            if (data[i] & self.tls_client_hello_mask[i]) != self.tls_client_hello_value[i]:
                return None, None, None, []

        # bounds checking
        record_length = int.from_bytes(data[3:5], byteorder='big')
        if record_length != len(data[5:]):
            return None, None, None, []

        # extract fingerprint string
        fp_str_, server_name = self.extract_fingerprint(data[5:])
        if fp_str_ == None:
            return None, None, None, []
        approx_str_ = None

        # fingerprint approximate matching if necessary
        if fp_str_ not in self.fp_db:
            lit_fp = eval_fp_str(fp_str_)
            approx_str_ = self.find_approx_match(lit_fp)
            if approx_str_ == None:
                fp_ = self.gen_unknown_fingerprint(fp_str_)
                self.fp_db[fp_str_] = fp_
                return None, None, None, []
            self.fp_db[fp_str_] = self.fp_db[approx_str_]
            self.fp_db[fp_str_]['approx_str'] = approx_str_
        if 'approx_str' in self.fp_db[fp_str_]:
            approx_str_ = self.fp_db[fp_str_]['approx_str']

        return 'tls', fp_str_, approx_str_, [{'name':'server_name', 'data':server_name}]



    def proc_identify(self, fp_str_, context_, dest_addr, dest_port, list_procs=0):
        server_name = None
        # extract server_name field from context object
        if context_ != None:
            for x_ in context_:
                if x_['name'] == 'server_name':
                    server_name = x_['data']
                    break

        # perform process identification given the fingerprint string and destination information
        result = self.identify(fp_str_, server_name, dest_addr, dest_port, list_procs)

        return result


    @functools.lru_cache(maxsize=MAX_CACHED_RESULTS)
    def identify(self, fp_str_, server_name, dest_addr, dest_port, list_procs=0):
        fp_ = self.get_database_entry(fp_str_, None)
        if fp_ == None:
            # if malware data is in the database, report malware scores
            if self.MALWARE_DB:
                return {'process': 'Unknown', 'score': 0.0, 'malware': False, 'p_malware': 0.0}
            else:
                return {'process': 'Unknown', 'score': 0.0}

        # find generalized classes for destination information
        domain, tld = get_tld_info(server_name)
        asn = get_asn_info(dest_addr)
        port_app = get_port_application(dest_port)
        features = [asn, domain, port_app]

        # compute and sort scores for each process in the fingerprint
        fp_tc = fp_['total_count']
        r_ = [self.compute_score(features, p_, fp_tc) for p_ in fp_['process_info']]
        r_ = sorted(r_, key=operator.itemgetter('score'), reverse=True)

        # if score == 0 or no match could be found, return default process
        if len(r_) == 0 or r_[0]['score'] == 0.0:
            predict_ = fp_['process_info'][0]['process'].decode()
            predict_ = self.app_families[predict_] if predict_ in self.app_families else predict_
            if self.MALWARE_DB:
                return {'process':predict_, 'score': 0.0, 'malware': fp_['process_info'][0]['malware'], 'p_malware': 0.0}
            else:
                return {'process':predict_, 'score':0.0}

        # in the case of malware, remove pseudo process meant to reduce false positives
        if self.MALWARE_DB and r_[0]['malware'] == False and \
           r_[0]['process'] == b'Generic DMZ Traffic' and len(r_) > 1 and r_[1]['malware'] == False:
            r_.pop(0)

        # get generalized process name if available
        process_name = r_[0]['process'].decode()
        process_name = self.app_families[process_name] if process_name in self.app_families else process_name

        # package the most probable process
        score_sum_ = sum([x_['score'] for x_ in r_])
        if self.MALWARE_DB:
            malware_score_ = sum([x_['score'] for x_ in r_ if x_['malware'] == 1])/score_sum_
            out_ = {'process':process_name, 'score':r_[0]['score'], 'malware':r_[0]['malware'], 'p_malware':malware_score_}
        else:
            out_ = {'process':process_name, 'score':r_[0]['score']}

        # return the top-n most probable processes is list_procs > 0
        if list_procs > 0:
            r_proc_ = r_[0:list_procs]
            for p_ in r_proc_:
                p_['score'] /= score_sum_
            out_['probable_processes'] = r_proc_
        out_['score'] /= score_sum_

        return out_


    def compute_score(self, features, p_, fp_tc_):
        p_count = p_['count']
        prob_process_given_fp = log(p_count/fp_tc_)

        base_prior_ = -18.42068 # log(1e-8)
        prior_      =  -4.60517 # log(1e-2)

        score_ = prob_process_given_fp*3 if prob_process_given_fp > base_prior_ else base_prior_*3

        if features[0] in p_['classes_ip_as']:
            tmp_ = log(p_['classes_ip_as'][features[0]]/p_count)
            score_ += tmp_ if tmp_ > prior_ else prior_
        else:
            score_ += base_prior_

        if features[1] in p_['classes_hostname_domains']:
            tmp_ = log(p_['classes_hostname_domains'][features[1]]/p_count)
            score_ += tmp_ if tmp_ > prior_ else prior_
        else:
            score_ += base_prior_

        if features[2] in p_['classes_port_applications']:
            tmp_ = log(p_['classes_port_applications'][features[2]]/p_count)
            score_ += tmp_ if tmp_ > prior_ else prior_
        else:
            score_ += base_prior_

        if self.MALWARE_DB:
            return {'score':exp(score_), 'process':p_['process'], 'sha256':p_['sha256'], 'malware':p_['malware']}
        else:
            return {'score':exp(score_), 'process':p_['process'], 'sha256':p_['sha256']}


    @functools.lru_cache(maxsize=MAX_CACHED_RESULTS)
    def get_database_entry(self, fp_str, approx_fp_str):
        fp_str_ = fp_str
        if approx_fp_str != None:
            fp_str_ = approx_fp_str
        
        if fp_str_ not in self.fp_db:
            return None

        return self.fp_db[fp_str_]


    def find_approx_match(self, tls_features, fp_str=None, source_filter=None, key_filter=None):
        target_ = get_sequence(tls_features)
        tls_params_ = get_tls_params(tls_features)

        t_sim_set = []
        approx_matches_set = self.find_approximate_matches_set(tls_params_, fp_str, source_filter, key_filter)
        for _,k in approx_matches_set:
            tmp_lit_fp = eval_fp_str(self.fp_db[k]['str_repr'])
            test_ = get_sequence(tmp_lit_fp)
            score_ = self.aligner.align(target_, test_)
            t_sim_set.append((1.0-2*score_/float(len(target_)+len(test_)), k))

        t_sim_set.sort()
        if len(t_sim_set) == 0:
            return None
        if t_sim_set[0][0] < 0.1:
            return t_sim_set[0][1]
        else:
            return None


    def find_approximate_matches_set(self, tls_params, fp_str=None, source_filter=None, key_filter=None):
        t_scores = []
        p0_ = set(tls_params[0])
        p1_ = set(tls_params[1])
        for k in self.fp_db.keys():
            k = k
            if source_filter != None and source_filter not in self.fp_db[k]['source']:
                continue
            if fp_str != None and key_filter != None and fp_str not in key_filter and k not in key_filter:
                continue
            if k not in self.tls_params_db:
                lit_fp = eval_fp_str(k)
                tls_params_ = get_tls_params(lit_fp)
                self.tls_params_db[k] = tls_params_
            q0_ = set(self.tls_params_db[k][0])
            q1_ = set(self.tls_params_db[k][1])
            s0_ = len(p0_.intersection(q0_))/max(1.0,len(p0_.union(q0_)))
            s1_ = len(p1_.intersection(q1_))/max(1.0,len(p1_.union(q1_)))
            s_ = s0_ + s1_
            t_scores.append((s_, k))
        t_scores.sort()
        t_scores.reverse()
        return t_scores[0:10]


    @functools.lru_cache(maxsize=MAX_CACHED_RESULTS)
    def gen_unknown_fingerprint(self, fp_str_):
        fp_ = {}
        fp_['str_repr'] = fp_str_
        lit_fp = eval_fp_str(fp_str_)
        if len(lit_fp) < 2 or len(lit_fp[1]) < 1:
            fp_['error'] = 'fingerprint string parsing error'
            return fp_
        max_imp, min_imp = get_implementation_date(lit_fp[1][0])
        fp_['max_implementation_date'] = max_imp
        fp_['min_implementation_date'] = min_imp
        fp_['total_count'] = 1
        fp_['process_info'] = [{'process': b'Unknown', 'sha256':'Unknown', 'count':1, 'malware': 0,
                                'classes_ip_as':{},'classes_hostname_tlds':{},'classes_hostname_domains':{},
                                'classes_port_applications':{},'os_info':{}}]

        return fp_


    def extract_fingerprint(self, data):
        data_len = len(data)

        # extract handshake version
        fp_ = b'(' + hexlify(data[4:6]) + b')'

        # skip header/client_random
        offset = 38

        # parse/skip session_id
        session_id_length = int.from_bytes(data[offset:offset+1], byteorder='big')
        offset += 1 + session_id_length
        if data_len - offset <= 0:
            return None, None

        # parse/extract/skip cipher_suites length
        cipher_suites_length = int.from_bytes(data[offset:offset+2], byteorder='big')
        offset += 2
        if data_len - offset <= 0:
            return None, None

        # parse/extract/skip cipher_suites
        fp_ += b'('
        fp_ += degrease_type_code(data, offset)
        if cipher_suites_length > 2:
            fp_ += hexlify(data[offset+2:offset+cipher_suites_length])
        fp_ += b')'
        offset += cipher_suites_length
        if data_len - offset <= 0:
            return None, None

        # parse/skip compression method
        compression_methods_length = int.from_bytes(data[offset:offset+1], byteorder='big')
        offset += 1 + compression_methods_length
        if data_len - offset <= 0:
            return fp_, None

        # parse/skip extensions length
        ext_total_len = int.from_bytes(data[offset:offset+2], byteorder='big')
        offset += 2
        if data_len - offset <= 0:
            return None, None

        # parse/extract/skip extension type/length/values
        fp_ += b'('
        ext_fp_len_ = 0
        server_name = None
        while ext_total_len > 0:
            fp_ += b'('
            if data_len - offset <= 0:
                return None, None

            # extract server name for process/malware identification
            if int.from_bytes(data[offset:offset+2], byteorder='big') == 0:
                server_name = extract_server_name(data[offset+2:])

            tmp_fp_ext, offset, ext_len = parse_extension(data, offset)
            fp_ += tmp_fp_ext
            ext_fp_len_ += len(tmp_fp_ext)

            ext_total_len -= 4 + ext_len
            fp_ += b')'
        fp_ += b')'

        return fp_, server_name


    def get_human_readable(self, fp_str_):
        lit_fp = eval_fp_str(fp_str_)
        fp_h = OrderedDict({})
        fp_h['version'] = get_version_from_str(lit_fp[0][0])
        fp_h['cipher_suites'] = get_cs_from_str(lit_fp[1][0])
        fp_h['extensions'] = []
        if len(lit_fp) > 2:
            fp_h['extensions'] = get_ext_from_str(lit_fp[2])

        return fp_h
