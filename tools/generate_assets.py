#!/usr/bin/env python3
"""
Generate retro Sony Walkman / cassette themed LiveArea assets for PSVitaman:
- sce_sys/icon0.png (128x128, 32-bit RGBA)
- sce_sys/livearea/contents/bg.png (840x500, 32-bit RGBA)
- sce_sys/livearea/contents/startup.png (280x158, 32-bit RGBA)
"""

import math
from PIL import Image, ImageDraw, ImageFont

def draw_cassette(draw, box, shell_color=(38, 43, 54), label_color=(235, 230, 218), spool_color=(240, 240, 240)):
    x0, y0, x1, y1 = box
    w = x1 - x0
    h = y1 - y0
    
    # Outer cassette body with rounded corners
    draw.rounded_rectangle([x0, y0, x1, y1], radius=max(4, int(w * 0.04)), fill=shell_color, outline=(20, 22, 28), width=max(1, int(w * 0.015)))
    
    # Cassette screws at corners
    screw_r = max(2, int(w * 0.02))
    screw_offsets = [(0.06, 0.08), (0.94, 0.08), (0.06, 0.92), (0.94, 0.92), (0.5, 0.92)]
    for ox, oy in screw_offsets:
        sx = x0 + int(w * ox)
        sy = y0 + int(h * oy)
        draw.ellipse([sx - screw_r, sy - screw_r, sx + screw_r, sy + screw_r], fill=(160, 165, 175), outline=(100, 105, 115))
        draw.line([sx - screw_r + 1, sy, sx + screw_r - 1, sy], fill=(80, 85, 95), width=1)
    
    # Label strip
    lx0 = x0 + int(w * 0.10)
    ly0 = y0 + int(h * 0.12)
    lx1 = x1 - int(w * 0.10)
    ly1 = y1 - int(h * 0.18)
    draw.rounded_rectangle([lx0, ly0, lx1, ly1], radius=max(3, int(w * 0.02)), fill=label_color, outline=(180, 175, 160), width=1)
    
    # Red & Blue retro Walkman accent stripe on label
    st_y = ly0 + int((ly1 - ly0) * 0.18)
    st_h = max(2, int(h * 0.04))
    draw.rectangle([lx0 + 2, st_y, lx1 - 2, st_y + st_h // 2], fill=(220, 60, 50))
    draw.rectangle([lx0 + 2, st_y + st_h // 2, lx1 - 2, st_y + st_h], fill=(30, 100, 200))
    
    # Cassette center transparent window
    wx0 = x0 + int(w * 0.22)
    wy0 = y0 + int(h * 0.38)
    wx1 = x1 - int(w * 0.22)
    wy1 = y1 - int(h * 0.30)
    draw.rounded_rectangle([wx0, wy0, wx1, wy1], radius=max(2, int(w * 0.02)), fill=(25, 28, 35), outline=(70, 75, 85), width=1)
    
    # Center magnetic tape bridge
    cx = (wx0 + wx1) // 2
    tw = int(w * 0.16)
    draw.rectangle([cx - tw // 2, wy0 + 2, cx + tw // 2, wy1 - 2], fill=(45, 30, 20)) # brown magnetic tape
    
    # Two spools
    spool_r = max(5, int(w * 0.08))
    spool_left_x = wx0 + int((wx1 - wx0) * 0.24)
    spool_right_x = wx0 + int((wx1 - wx0) * 0.76)
    spool_y = (wy0 + wy1) // 2
    
    for sx in [spool_left_x, spool_right_x]:
        # Outer tape roll
        tape_r = int(spool_r * 1.5)
        draw.ellipse([sx - tape_r, spool_y - tape_r, sx + tape_r, spool_y + tape_r], fill=(55, 38, 25), outline=(35, 25, 18))
        # White plastic hub
        draw.ellipse([sx - spool_r, spool_y - spool_r, sx + spool_r, spool_y + spool_r], fill=spool_color, outline=(180, 180, 180))
        # Center hole
        hole_r = max(2, int(spool_r * 0.4))
        draw.ellipse([sx - hole_r, spool_y - hole_r, sx + hole_r, spool_y + hole_r], fill=(25, 28, 35))
        # Spool teeth/spokes
        for a in range(0, 360, 60):
            rad = math.radians(a)
            tx = sx + int(math.cos(rad) * (spool_r - 2))
            ty = spool_y + int(math.sin(rad) * (spool_r - 2))
            draw.line([sx, spool_y, tx, ty], fill=(160, 160, 160), width=1)

def generate_icon0():
    img = Image.new("RGBA", (128, 128), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Background circle with Walkman blue / slate gradient look
    draw.rounded_rectangle([2, 2, 126, 126], radius=28, fill=(24, 30, 42), outline=(30, 215, 96), width=3) # Spotify green ring accent
    
    # Mini cassette
    draw_cassette(draw, [14, 28, 114, 98], shell_color=(45, 52, 65), label_color=(240, 238, 230))
    
    # Top LED indicator / mini brand text
    draw.text((32, 102), "PSVITAMAN", fill=(30, 215, 96))
    
    img.save("sce_sys/icon0.png", "PNG")
    print("Generated sce_sys/icon0.png (128x128)")

def generate_bg():
    img = Image.new("RGBA", (840, 500), (20, 24, 32, 255))
    draw = ImageDraw.Draw(img)
    
    # Brushed metal horizontal lines
    for y in range(0, 500, 4):
        draw.line([0, y, 840, y], fill=(23, 28, 38))
        
    # Walkman Classic Metallic Frame & Accent Bars
    draw.rectangle([0, 0, 840, 45], fill=(32, 38, 50))
    draw.rectangle([0, 45, 840, 48], fill=(30, 215, 96)) # Spotify green line
    draw.rectangle([0, 455, 840, 500], fill=(28, 34, 46))
    draw.rectangle([0, 452, 840, 455], fill=(70, 78, 95))
    
    # Title typography
    draw.text((40, 12), "SONY PS VITA  |  PSVITAMAN  •  SPOTIFY CASSETTE REMOTE", fill=(210, 215, 225))
    
    # Large detailed Cassette Deck in Center
    draw_cassette(draw, [180, 80, 660, 410], shell_color=(36, 42, 54), label_color=(242, 240, 232), spool_color=(250, 250, 250))
    
    # Cassette typography on label
    draw.text((260, 135), "PSVITAMAN  STEREO  CASSETTE", fill=(40, 40, 40))
    draw.text((550, 135), "TYPE I (NORMAL)", fill=(80, 80, 80))
    draw.text((260, 195), "A", fill=(180, 40, 30))
    draw.text((285, 195), "SPOTIFY REMOTE CONTROLLER", fill=(30, 30, 30))
    
    # Bottom HUD
    draw.text((40, 468), "STEREO DECK  •  960x544 VITA NATIVE  •  REMOTE PLAYBACK VIA SPOTIFY CONNECT", fill=(140, 150, 170))
    
    img.save("sce_sys/livearea/contents/bg.png", "PNG")
    print("Generated sce_sys/livearea/contents/bg.png (840x500)")

def generate_startup():
    img = Image.new("RGBA", (280, 158), (28, 34, 46, 255))
    draw = ImageDraw.Draw(img)
    
    # Outer frame
    draw.rounded_rectangle([3, 3, 277, 155], radius=12, outline=(30, 215, 96), width=2)
    
    # Top Walkman badge
    draw.rectangle([10, 10, 270, 32], fill=(20, 24, 34))
    draw.text((24, 14), "PSVITAMAN CASSETTE DECK", fill=(30, 215, 96))
    
    # Cassette graphic
    draw_cassette(draw, [40, 42, 240, 118], shell_color=(38, 44, 56), label_color=(240, 238, 232))
    
    # Start button pill
    draw.rounded_rectangle([65, 126, 215, 150], radius=8, fill=(30, 215, 96), outline=(20, 180, 80))
    draw.text((88, 130), "▶  START REMOTE", fill=(10, 25, 15))
    
    img.save("sce_sys/livearea/contents/startup.png", "PNG")
    print("Generated sce_sys/livearea/contents/startup.png (280x158)")

if __name__ == "__main__":
    generate_icon0()
    generate_bg()
    generate_startup()
