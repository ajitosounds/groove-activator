"""Uploads a folder to Dropbox, replacing files that are already there.

Needs DROPBOX_APP_KEY, DROPBOX_APP_SECRET and DROPBOX_REFRESH_TOKEN in the environment.
Usage: python dropbox_upload.py <local folder> <dropbox folder, e.g. "/AJT JOY16">
"""
import json, os, sys, urllib.parse, urllib.request, base64

CHUNK = 64 * 1024 * 1024  # files over 150 MB must use an upload session

def token():
    key, secret = os.environ['DROPBOX_APP_KEY'], os.environ['DROPBOX_APP_SECRET']
    data = urllib.parse.urlencode({'grant_type': 'refresh_token', 'refresh_token': os.environ['DROPBOX_REFRESH_TOKEN']}).encode()
    req = urllib.request.Request('https://api.dropboxapi.com/oauth2/token', data=data)
    req.add_header('Authorization', 'Basic ' + base64.b64encode(f'{key}:{secret}'.encode()).decode())
    return json.load(urllib.request.urlopen(req))['access_token']

def call(tok, endpoint, arg, body=b''):
    req = urllib.request.Request('https://content.dropboxapi.com/2/' + endpoint, data=body)
    req.add_header('Authorization', 'Bearer ' + tok)
    req.add_header('Content-Type', 'application/octet-stream')
    # Dropbox-API-Arg must be ASCII: escape everything else (Japanese file names)
    req.add_header('Dropbox-API-Arg', json.dumps(arg, ensure_ascii=True))
    return json.load(urllib.request.urlopen(req))

def upload(tok, local, remote):
    size = os.path.getsize(local)
    commit = {'path': remote, 'mode': 'overwrite', 'mute': True}
    with open(local, 'rb') as f:
        if size <= CHUNK:
            call(tok, 'files/upload', commit, f.read()); return
        sid = call(tok, 'files/upload_session/start', {'close': False}, f.read(CHUNK))['session_id']
        while True:
            chunk = f.read(CHUNK); offset = f.tell() - len(chunk)
            if f.tell() >= size:
                call(tok, 'files/upload_session/finish', {'cursor': {'session_id': sid, 'offset': offset}, 'commit': commit}, chunk); return
            call(tok, 'files/upload_session/append_v2', {'cursor': {'session_id': sid, 'offset': offset}, 'close': False}, chunk)

def main(src, dest):
    tok = token()
    for root, _, files in os.walk(src):
        for name in sorted(files):
            local = os.path.join(root, name)
            remote = dest.rstrip('/') + '/' + os.path.relpath(local, src).replace(os.sep, '/')
            print('upload', remote, os.path.getsize(local), 'bytes', flush=True)
            upload(tok, local, remote)

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
