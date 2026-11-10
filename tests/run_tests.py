#!/usr/bin/env python3
"""无需真实数据库的 Session、处理函数、前端和 HTTP 集成回归检查。"""
import http.client
import json
import os
from pathlib import Path
import shlex
import shutil
import socket
import struct
import subprocess
import tempfile
import time
import zlib

ROOT = Path(__file__).resolve().parents[1]


def run(*command):
    subprocess.run(command, cwd=ROOT, check=True)


def check_http(binary, workdir):
    with socket.socket() as probe:
        probe.bind(('127.0.0.1', 0))
        port = probe.getsockname()[1]
    server = subprocess.Popen([str(binary), '--serve', str(port)], cwd=workdir,
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def request(method, path, body=None, cookie=None, content_type=None):
        headers = {'Accept': 'application/json', 'Connection': 'close'}
        if body is not None:
            if isinstance(body, bytes):
                headers['Content-Type'] = content_type or 'image/png'
            else:
                body = json.dumps(body).encode()
                headers['Content-Type'] = content_type or 'application/json'
        if cookie:
            headers['Cookie'] = cookie
        conn = http.client.HTTPConnection('127.0.0.1', port, timeout=3)
        try:
            conn.request(method, path, body=body, headers=headers)
            rsp = conn.getresponse()
            return rsp.status, dict(rsp.getheaders()), rsp.read()
        finally:
            conn.close()

    try:
        for _ in range(100):
            if server.poll() is not None:
                raise RuntimeError('测试 HTTP 服务提前退出')
            try:
                request('GET', '/api/me')
                break
            except OSError:
                time.sleep(0.05)
        else:
            raise RuntimeError('测试 HTTP 服务启动超时')

        for name in ('index.html', 'questions.html', 'question.html', 'user.html',
                     'session.js', 'session.css', 'progress.js', 'progress.css',
                     'avatar.js', 'avatar.css', 'avaters/commen_path.png'):
            status, _, body = request('GET', '/' + name)
            assert status == 200 and body == (ROOT / 'wwwroot' / name).read_bytes(), name
        for method, path in (('GET', '/api/me'), ('GET', '/api/showuser'), ('GET', '/api/allquestions'),
                             ('GET', '/api/question/1'), ('POST', '/api/question/1/judge'),
                             ('POST', '/api/progress/save'), ('POST', '/api/avatar')):
            status, headers, body = request(method, path)
            assert status == 401 and json.loads(body)['success'] is False
            assert 'Location' not in headers and headers['Cache-Control'] == 'no-store'

        credentials = {'idcount': 'student', 'idpasswd': 'test-password'}
        status, _, _ = request('POST', '/api/login', {**credentials, 'idpasswd': 'wrong'})
        assert status == 401
        status, headers, body = request('POST', '/api/login', credentials, 'sessionid=stale')
        assert status == 200 and json.loads(body)['success'] is True
        cookie = headers['Set-Cookie'].split(';')[0]
        assert len(cookie.split('=')[1]) == 64
        for attribute in ('Path=/', 'HttpOnly', 'SameSite=Lax', 'Max-Age=1800'):
            assert attribute in headers['Set-Cookie']
        status, _, body = request('GET', '/api/me', cookie='theme=a=b; ' + cookie)
        assert status == 200 and json.loads(body)['user']['name'] == '测试同学'
        assert 'passwd' not in body.decode()
        status, _, body = request('GET', '/api/showuser', cookie=cookie)
        assert status == 200 and json.loads(body)['user'] == {
            'idcount': 'student', 'name': '测试同学', 'identity': 'student',
            'avater': './wwwroot/avaters/commen_path.png'}
        status, _, _ = request('GET', '/api/me', cookie='sessionid=forged')
        assert status == 401

        # 只在临时 wwwroot 中写入头像，不触碰开发目录的用户图片。
        def chunk(kind, data):
            return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
        png = (b'\x89PNG\r\n\x1a\n'
               + chunk(b'IHDR', struct.pack('>IIBBBBB', 1, 1, 8, 6, 0, 0, 0))
               + chunk(b'IDAT', zlib.compress(b'\x00\x24\x4b\x3b\xff'))
               + chunk(b'IEND', b''))
        assert request('POST', '/api/avatar', b'', cookie)[0] == 400
        assert request('POST', '/api/avatar', b'not a png', cookie)[0] == 415
        assert request('POST', '/api/avatar', png, cookie, 'image/jpeg')[0] == 415
        assert request('POST', '/api/avatar', b'x' * (10 * 1024 * 1024), cookie)[0] == 415
        assert request('POST', '/api/avatar', b'x' * (10 * 1024 * 1024 + 1), cookie)[0] == 413
        status, _, body = request('POST', '/api/avatar', png, cookie)
        assert status == 200 and json.loads(body)['success'] is True
        assert (workdir / 'wwwroot/avaters/42.png').read_bytes() == png
        status, headers, body = request('GET', '/avaters/42.png')
        assert status == 200 and body == png and headers['Content-Type'] == 'image/png'
        status, _, body = request('GET', '/api/showuser', cookie=cookie)
        assert status == 200 and json.loads(body)['user']['avater'] == './wwwroot/avaters/42.png'

        status, headers, _ = request('POST', '/api/login', credentials, cookie)
        new_cookie = headers['Set-Cookie'].split(';')[0]
        assert status == 200 and new_cookie != cookie
        status, _, body = request('GET', '/api/showuser', cookie=new_cookie)
        assert status == 200 and json.loads(body)['user']['avater'] == './wwwroot/avaters/42.png'
        assert request('GET', '/api/me', cookie=cookie)[0] == 401
        status, headers, body = request('GET', '/api/allquestions', cookie=new_cookie)
        assert status == 200 and len(json.loads(body)['questions']) == 1
        assert 'Set-Cookie' not in headers
        status, _, body = request('GET', '/api/question/1', cookie=new_cookie)
        assert status == 200 and 'answer' not in json.loads(body)['question']
        for answer, expected in [(' a ', True), ('B', False)]:
            status, _, body = request('POST', '/api/question/1/judge', {'answer': answer}, new_cookie)
            assert status == 200 and json.loads(body)['correct'] is expected
        status, _, body = request('GET', '/api/allquestions', cookie=new_cookie)
        progress = json.loads(body)['questions'][0]
        assert status == 200 and progress['attempt'] == 2 and progress['correct_count'] == 1
        assert progress['last_answer'] == 'B' and progress['last_correct'] is False
        assert request('POST', '/api/question/1/judge', {'answer': '  '}, new_cookie)[0] == 400
        status, _, body = request('POST', '/api/progress/save', cookie=new_cookie)
        assert status == 200 and json.loads(body)['saved_count'] == 1
        status, _, body = request('POST', '/api/progress/save', cookie=new_cookie)
        assert status == 200 and json.loads(body)['saved_count'] == 0

        # 另一个客户端登录不会撤销当前客户端；退出只撤销对应会话。
        _, headers, _ = request('POST', '/api/login', credentials)
        second_cookie = headers['Set-Cookie'].split(';')[0]
        status, _, body = request('GET', '/api/allquestions', cookie=second_cookie)
        assert status == 200 and json.loads(body)['questions'][0]['attempt'] == 2
        assert json.loads(body)['questions'][0]['correct_count'] == 1
        status, headers, _ = request('POST', '/api/logout', cookie=new_cookie)
        assert status == 200 and 'Max-Age=0' in headers['Set-Cookie']
        assert request('GET', '/api/me', cookie=new_cookie)[0] == 401
        assert request('GET', '/api/showuser', cookie=new_cookie)[0] == 401
        assert request('GET', '/api/me', cookie=second_cookie)[0] == 200
        assert request('POST', '/api/logout', cookie=new_cookie)[0] == 200

        registration = {'name': '新同学', 'idcount': 'new', 'idpasswd': 'test'}
        assert request('POST', '/api/register', registration)[0] == 200
        assert request('POST', '/api/register', {**registration, 'identity': 'teacher'})[0] == 403
        print('HTTP transport: static assets, PNG upload/validation, avatar persistence, auth, judging and save passed', flush=True)
    finally:
        server.terminate()
        try:
            server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait()


def main():
    compiler = shlex.split(os.environ.get('CXX', 'g++'))
    with tempfile.TemporaryDirectory(prefix='jiao-auth-tests-') as tmp:
        session_test = Path(tmp) / 'session_test'
        http_test = Path(tmp) / 'http_test'
        run(*compiler, '-std=c++17', '-pthread', 'tests/session_test.cc', '-o', str(session_test))
        run(str(session_test))
        run(*compiler, '-std=c++17', '-pthread', 'tests/http_handlers_test.cc',
            '-ljsoncpp', '-lmysqlclient', '-o', str(http_test))
        run(str(http_test))
        run('node', 'tests/frontend_test.js')
        workdir = Path(tmp) / 'web'
        workdir.mkdir()
        shutil.copytree(ROOT / 'wwwroot', workdir / 'wwwroot',
                        ignore=lambda directory, names: [name for name in names if name[:-4].isdigit() and name.endswith('.png')])
        check_http(http_test, workdir)
    print('All checks passed. No real database was accessed.', flush=True)


if __name__ == '__main__':
    main()
