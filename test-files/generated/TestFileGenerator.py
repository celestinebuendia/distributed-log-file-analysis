"""
Modified code originating from code by Vishnu U on Kaggle (CCO: Public Domain)
Original Link: https://www.kaggle.com/datasets/vishnu0399/server-logs
"""
import random
from random import choice, choices
import time
import faker
import os
import argparse
os.environ['TZ'] = 'Asia/Kolkata'
fak = faker.Faker()

parser = argparse.ArgumentParser(description='Generate synthetic server log files')
parser.add_argument('n_lines', type=int, help='Number of log lines to generate')
parser.add_argument('filename', help='Output log file name')
args = parser.parse_args()

def generate_userbase(n_users):
    users = set()
    
    while len(users) < n_users:
        ip = fak.ipv4_public()
        users.add(ip)
    
    return list(users)

user_ips = generate_userbase(5000)
heavy_users = user_ips[:500]   # 10% most active users 
light_users = user_ips[500:]

def str_time_prop(start, end, format, prop):
    stime = time.mktime(time.strptime(start, format))
    etime = time.mktime(time.strptime(end, format))
    ptime = stime + prop * (etime - stime)
    return time.strftime(format, time.localtime(ptime))

def random_date(start, end):
    # Beta distribution skews timestamps toward "business hours"
    prop = random.betavariate(2, 2)
    return str_time_prop(start, end, '%d/%b/%Y:%I:%M:%S %z', prop)

REQUESTS = ['GET', 'POST', 'PUT', 'DELETE']
REQUEST_WEIGHTS = [70, 20, 7, 3]

ENDPOINTS = ['/usr', '/usr/admin', '/usr/admin/developer', '/usr/login', '/usr/register', '/catalog', '/catalog/search', '/help']
ENDPOINT_WEIGHTS = [5, 2, 1, 15, 10, 25, 30, 12]

STATUS_CODES = ['200', '303', '404', '500', '403', '502', '304']
STATUS_CODE_WEIGHTS = [80, 5, 10, 2, 1, 1, 1]

UA_LIST = ['Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:84.0) Gecko/20100101 Firefox/84.0',
           'Mozilla/5.0 (Android 10; Mobile; rv:84.0) Gecko/84.0 Firefox/84.0',
           'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/87.0.4280.141 Safari/537.36',
           'Mozilla/5.0 (Linux; Android 10; ONEPLUS A6000) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/87.0.4280.141 Mobile Safari/537.36',
           'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/89.0.4380.0 Safari/537.36 Edg/89.0.759.0',
           'Mozilla/5.0 (Linux; Android 10; ONEPLUS A6000) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/77.0.3865.116 Mobile Safari/537.36 EdgA/45.12.4.5121',
           'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/87.0.4280.88 Safari/537.36 OPR/73.0.3856.329',
           'Mozilla/5.0 (Linux; Android 10; ONEPLUS A6000) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/86.0.4240.198 Mobile Safari/537.36 OPR/61.2.3076.56749',
           'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_9_3) AppleWebKit/537.75.14 (KHTML, like Gecko) Version/7.0.3 Safari/7046A194A',
           'Mozilla/5.0 (iPhone; CPU iPhone OS 12_4_9 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/12.1.2 Mobile/15E148 Safari/604.1']
UA_WEIGHTS = [20, 8, 25, 10, 12, 5, 8, 4, 5, 3]

f = open(args.filename, "w")
for _ in range(args.n_lines):
    if random.random() < 0.6:
        ip_choice = random.choice(heavy_users)
    else:
        ip_choice = random.choice(light_users)

    if random.random() < 0.5:
        referer_choice = '-'
    else:
        referer_choice = fak.uri()

    endpoint = choices(ENDPOINTS, weights=ENDPOINT_WEIGHTS)[0]
    request = choices(['GET', 'POST', 'PUT', 'DELETE'], weights=REQUEST_WEIGHTS)[0]
    status = choices(STATUS_CODES, weights=STATUS_CODE_WEIGHTS)[0]
    ua = choices(UA_LIST, weights=UA_WEIGHTS)[0]

    f.write('%s - - [%s] "%s %s HTTP/1.0" %s %s "%s" "%s" %s\n' % 
        (ip_choice,
         random_date("01/Jan/2025:12:00:00 +0530","01/Jan/2026:12:00:00 +0530"), 
         request,
         endpoint,
         status,    
         str(int(random.gauss(5000,50))),
         referer_choice,
         ua,
         random.randint(1,5000)))

f.close()