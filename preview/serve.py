#!/usr/bin/env python3
"""Static file server + tiny proxy for CelesTrak TLE endpoints.

CelesTrak doesn't send CORS headers, so browser fetch is blocked.
We proxy /tle/iss and /tle/css here to dodge CORS in local dev.
"""
import http.server
import socketserver
import urllib.request
from urllib.error import URLError

PORT = 8765

TLE_URLS = {
    '/tle/iss': 'https://celestrak.org/NORAD/elements/gp.php?CATNR=25544&FORMAT=TLE',
    '/tle/css': 'https://celestrak.org/NORAD/elements/gp.php?CATNR=48274&FORMAT=TLE',
    '/crew':    'http://api.open-notify.org/astros.json',
}

class Handler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path in TLE_URLS:
            try:
                with urllib.request.urlopen(TLE_URLS[self.path], timeout=10) as r:
                    body = r.read()
                ctype = 'application/json' if self.path.endswith('.json') or 'astros' in TLE_URLS[self.path] else 'text/plain; charset=utf-8'
                self.send_response(200)
                self.send_header('Content-Type', ctype)
                self.send_header('Access-Control-Allow-Origin', '*')
                self.send_header('Cache-Control', 'public, max-age=60')
                self.end_headers()
                self.wfile.write(body)
            except URLError as e:
                self.send_response(502)
                self.send_header('Content-Type', 'text/plain')
                self.end_headers()
                self.wfile.write(f'TLE proxy error: {e}'.encode())
            return
        return super().do_GET()

if __name__ == '__main__':
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(('', PORT), Handler) as httpd:
        print(f'serving on http://localhost:{PORT}')
        httpd.serve_forever()
