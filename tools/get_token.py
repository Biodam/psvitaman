#!/usr/bin/env python3
"""
PSVitaman - Spotify Refresh Token Generator Helper
Run this script on your PC to authorize your Spotify Developer App and obtain a refresh token
for use with PSVitaman on your PlayStation Vita.

Requirements:
    pip install requests
"""

import sys
import base64
import urllib.parse
import webbrowser
from http.server import HTTPServer, BaseHTTPRequestHandler

REDIRECT_PORT = 8888
REDIRECT_URI = f"http://127.0.0.1:{REDIRECT_PORT}/callback"
AUTH_CODE = None

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
    print("=" * 65)
    print("         PSVitaman - Spotify Token Generator")
    print("=" * 65)
    print("Step 1: Go to https://developer.spotify.com/dashboard")
    print("Step 2: Create an App (or open an existing one).")
    print("Step 3: In App Settings, add this Redirect URI exactly:")
    print(f"        {REDIRECT_URI}")
    print("=" * 65)
    
    client_id = input("\nEnter your Client ID: ").strip()
    if not client_id:
        print("Error: Client ID is required.")
        return
        
    client_secret = input("Enter your Client Secret: ").strip()
    if not client_secret:
        print("Error: Client Secret is required.")
        return

    scopes = [
        "user-read-playback-state",
        "user-modify-playback-state",
        "user-read-currently-playing"
    ]
    scope_str = "%20".join(scopes)
    
    auth_url = (
        f"https://accounts.spotify.com/authorize?"
        f"client_id={client_id}&response_type=code&"
        f"redirect_uri={urllib.parse.quote(REDIRECT_URI)}&"
        f"scope={scope_str}"
    )

    print("\nOpening your browser to authorize Spotify...")
    webbrowser.open(auth_url)

    print(f"Waiting for authorization on {REDIRECT_URI}...")
    server = HTTPServer(("127.0.0.1", REDIRECT_PORT), OAuthHandler)
    while AUTH_CODE is None:
        server.handle_request()

    # Now exchange code for refresh token
    import urllib.request
    import json

    token_url = "https://accounts.spotify.com/api/token"
    auth_header = base64.b64encode(f"{client_id}:{client_secret}".encode()).decode()
    
    data = urllib.parse.urlencode({
        "grant_type": "authorization_code",
        "code": AUTH_CODE,
        "redirect_uri": REDIRECT_URI
    }).encode()
    
    req = urllib.request.Request(
        token_url,
        data=data,
        headers={
            "Authorization": f"Basic {auth_header}",
            "Content-Type": "application/x-www-form-urlencoded"
        }
    )

    try:
        with urllib.request.urlopen(req) as resp:
            tokens = json.loads(resp.read().decode())
            refresh_token = tokens.get("refresh_token")
            
            print("\n" + "=" * 65)
            print("                SUCCESS! Here is your config.ini")
            print("=" * 65)
            print("Save the following text into:")
            print("ux0:data/psvitaman/config.ini\n")
            print("[spotify]")
            print(f"client_id = {client_id}")
            print(f"client_secret = {client_secret}")
            print(f"refresh_token = {refresh_token}")
            print("=" * 65)
            
            save = input("\nWould you like to save this to a local 'config.ini' now? (y/n): ").strip().lower()
            if save == 'y':
                with open("config.ini", "w") as f:
                    f.write("[spotify]\n")
                    f.write(f"client_id = {client_id}\n")
                    f.write(f"client_secret = {client_secret}\n")
                    f.write(f"refresh_token = {refresh_token}\n")
                print("Saved to config.ini! Copy this file to ux0:data/psvitaman/config.ini on your PS Vita.")
    except Exception as e:
        print(f"\nFailed to obtain refresh token: {e}")

if __name__ == "__main__":
    main()
