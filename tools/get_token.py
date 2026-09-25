#!/usr/bin/env python3
"""
PSVitaman - Spotify Refresh Token Generator Helper
Run this script on your PC to authorize your Spotify Developer App and obtain a refresh token
for use with PSVitaman on your PlayStation Vita.
"""

import os
import sys
import base64
import hashlib
import secrets
import urllib.parse
import urllib.request
import json
import webbrowser
import ftplib
from http.server import HTTPServer, BaseHTTPRequestHandler

REDIRECT_PORT = 8888
REDIRECT_URI = f"http://127.0.0.1:{REDIRECT_PORT}/callback"
AUTH_CODE = None

def load_dotenv():
    """Load key-value pairs from .env in repository root."""
    env = {}
    env_path = os.path.join(os.path.dirname(__file__), "..", ".env")
    if os.path.exists(env_path):
        with open(env_path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                k, v = line.split("=", 1)
                env[k.strip()] = v.strip().strip("'\"")
    return env, env_path

def save_refresh_token_to_dotenv(env_path, refresh_token):
    """Save or update SPOTIFY_REFRESH_TOKEN in .env."""
    lines = []
    found = False
    if os.path.exists(env_path):
        with open(env_path, "r", encoding="utf-8") as f:
            for line in f:
                if line.strip().startswith("SPOTIFY_REFRESH_TOKEN=") or line.strip().startswith("# SPOTIFY_REFRESH_TOKEN="):
                    lines.append(f"SPOTIFY_REFRESH_TOKEN={refresh_token}\n")
                    found = True
                else:
                    lines.append(line)
    if not found:
        lines.append(f"\nSPOTIFY_REFRESH_TOKEN={refresh_token}\n")
    with open(env_path, "w", encoding="utf-8") as f:
        f.writelines(lines)

class OAuthHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        global AUTH_CODE
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == "/callback":
            params = urllib.parse.parse_qs(parsed.query)
            if "code" in params:
                AUTH_CODE = params["code"][0]
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.end_headers()
                self.wfile.write(b"""
                <html>
                <body style="font-family:sans-serif; text-align:center; padding:50px; background:#121212; color:#1DB954;">
                    <h1>Authorization Successful!</h1>
                    <p style="color:#ffffff;">You can close this tab and return to the terminal.</p>
                </body>
                </html>
                """)
            else:
                self.send_response(400)
                self.end_headers()
                self.wfile.write(b"Authorization failed.")
        else:
            self.send_response(404)
            self.end_headers()

    def log_message(self, format, *args):
        return  # Silence server logs

def main():
    global AUTH_CODE
    dotenv, env_path = load_dotenv()

    print("=" * 65)
    print("         PSVitaman - Spotify Token Generator")
    print("=" * 65)

    default_client_id = dotenv.get("SPOTIFY_CLIENT_ID", "")
    if default_client_id:
        print(f"Loaded Client ID from .env: {default_client_id}")
        user_cid = input(f"Press [ENTER] to use this Client ID (or type a new one): ").strip()
        client_id = user_cid if user_cid else default_client_id
    else:
        client_id = input("\nEnter your Client ID: ").strip()

    if not client_id:
        print("Error: Client ID is required.")
        return

    default_client_secret = dotenv.get("SPOTIFY_CLIENT_SECRET", "")
    if default_client_secret:
        print(f"Loaded Client Secret from .env.")
        user_secret = input("Press [ENTER] to use it (or type a new one): ").strip()
        client_secret = user_secret if user_secret else default_client_secret
    else:
        client_secret = input("Enter your Client Secret (Leave empty to use PKCE without secret): ").strip()

    use_pkce = len(client_secret) == 0

    scopes = [
        "user-read-playback-state",
        "user-modify-playback-state",
        "user-read-currently-playing"
    ]
    scope_str = "%20".join(scopes)

    # Generate PKCE verifier & challenge
    code_verifier = None
    code_challenge_params = ""
    if use_pkce:
        code_verifier = secrets.token_urlsafe(64)
        digest = hashlib.sha256(code_verifier.encode("utf-8")).digest()
        code_challenge = base64.urlsafe_b64encode(digest).decode("utf-8").rstrip("=")
        code_challenge_params = f"&code_challenge_method=S256&code_challenge={code_challenge}"

    auth_url = (
        f"https://accounts.spotify.com/authorize?"
        f"client_id={client_id}&response_type=code&"
        f"redirect_uri={urllib.parse.quote(REDIRECT_URI)}&"
        f"scope={scope_str}"
        f"{code_challenge_params}"
    )

    print("\nOpening your browser to authorize Spotify...")
    webbrowser.open(auth_url)

    print(f"Waiting for authorization callback on {REDIRECT_URI}...")
    server = HTTPServer(("127.0.0.1", REDIRECT_PORT), OAuthHandler)
    while AUTH_CODE is None:
        server.handle_request()

    # Exchange authorization code for refresh token
    token_url = "https://accounts.spotify.com/api/token"

    if use_pkce:
        data = urllib.parse.urlencode({
            "grant_type": "authorization_code",
            "code": AUTH_CODE,
            "redirect_uri": REDIRECT_URI,
            "client_id": client_id,
            "code_verifier": code_verifier
        }).encode()
        headers = {
            "Content-Type": "application/x-www-form-urlencoded"
        }
    else:
        auth_header = base64.b64encode(f"{client_id}:{client_secret}".encode()).decode()
        data = urllib.parse.urlencode({
            "grant_type": "authorization_code",
            "code": AUTH_CODE,
            "redirect_uri": REDIRECT_URI
        }).encode()
        headers = {
            "Authorization": f"Basic {auth_header}",
            "Content-Type": "application/x-www-form-urlencoded"
        }

    req = urllib.request.Request(token_url, data=data, headers=headers)

    try:
        with urllib.request.urlopen(req) as resp:
            tokens = json.loads(resp.read().decode())
            refresh_token = tokens.get("refresh_token")

            if not refresh_token:
                print("Error: No refresh_token returned by Spotify.")
                return

            print("\n" + "=" * 65)
            print("                SUCCESS! AUTHORIZATION COMPLETE")
            print("=" * 65)
            print("[spotify]")
            print(f"client_id = {client_id}")
            if client_secret:
                print(f"client_secret = {client_secret}")
            print(f"refresh_token = {refresh_token}")
            print("=" * 65)

            # Option A: Save to .env (build-in credentials so VPK works without files)
            bake = input("\nSave refresh_token to .env to build a zero-config VPK? (Y/n): ").strip().lower()
            if bake != 'n':
                save_refresh_token_to_dotenv(env_path, refresh_token)
                print("Saved refresh_token to .env! Any VPK built now will work without modifying files on Vita.")

            # Option B: Save local config.ini
            save = input("\nSave to local 'config.ini'? (y/N): ").strip().lower()
            if save == 'y':
                with open("config.ini", "w") as f:
                    f.write("[spotify]\n")
                    f.write(f"client_id = {client_id}\n")
                    f.write(f"client_secret = {client_secret}\n")
                    f.write(f"refresh_token = {refresh_token}\n")
                print("Saved to config.ini!")

            # Option C: Direct VitaShell FTP transfer
            ftp_opt = input("\nUpload config directly to PS Vita via VitaShell FTP? (y/N): ").strip().lower()
            if ftp_opt == 'y':
                vita_ip = input("Enter PS Vita IP address (e.g. 192.168.1.50): ").strip()
                vita_port = input("Enter FTP port [default 1337]: ").strip()
                port = int(vita_port) if vita_port else 1337

                config_content = (
                    f"[spotify]\n"
                    f"client_id = {client_id}\n"
                    f"client_secret = {client_secret}\n"
                    f"refresh_token = {refresh_token}\n"
                ).encode("utf-8")

                try:
                    ftp = ftplib.FTP()
                    ftp.connect(vita_ip, port, timeout=10)
                    ftp.login()
                    try:
                        ftp.mkd("ux0:/data/psvitaman")
                    except Exception:
                        pass
                    import io
                    ftp.storbinary("STOR ux0:/data/psvitaman/config.ini", io.BytesIO(config_content))
                    ftp.quit()
                    print(f"Successfully uploaded config.ini to ux0:data/psvitaman/config.ini on {vita_ip}:{port}!")
                except Exception as ftp_err:
                    print(f"FTP upload failed: {ftp_err}")

    except Exception as e:
        print(f"\nFailed to obtain refresh token: {e}")

if __name__ == "__main__":
    main()
