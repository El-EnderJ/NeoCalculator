import os
from PIL import Image, ImageDraw, ImageFont
import math

# Generate 120 mockup frames of a calculator assembling for GSAP
frames_dir = "public/frames"
os.makedirs(frames_dir, exist_ok=True)

width, height = 1920, 1080
center_x = width // 2
center_y = height // 2

# Base parameters for our "calculator" Mockup (TI-84 style but cyberpunk)
calc_w = 400
calc_h = 750

for i in range(120):
    progress = i / 119.0 # 0.0 to 1.0
    
    img = Image.new('RGB', (width, height), color='#0a0a0a')
    draw = ImageDraw.Draw(img)
    
    # Grid background for 'engineering' vibe
    for grid_x in range(0, width, 100):
        draw.line([(grid_x, 0), (grid_x, height)], fill='#1a1a1a', width=1)
    for grid_y in range(0, height, 100):
        draw.line([(0, grid_y), (width, grid_y)], fill='#1a1a1a', width=1)

    # Sequence logic:
    # 0.0 - 0.2: PCB appears and glows
    # 0.2 - 0.5: Screen locks in
    # 0.5 - 0.7: Case closes over
    # 0.7 - 1.0: Keyboard / Keys appear and neon accents glow

    # 1. PCB BASE
    pcb_alpha = min(255, int((progress / 0.1) * 255)) if progress > 0 else 0
    pcb_y_offset = (1.0 - min(1, progress/0.2)) * 300
    
    if progress > 0:
        draw.rounded_rectangle(
            [(center_x - calc_w//2, center_y - calc_h//2 + pcb_y_offset), 
             (center_x + calc_w//2, center_y + calc_h//2 + pcb_y_offset)],
            radius=20, fill='#0d230d', outline='#ccff00', width=2
        )
        # Fake circuits
        for c in range(10):
            draw.line([(center_x - 100, center_y + c*30 + pcb_y_offset), (center_x + 100, center_y + c*30 + pcb_y_offset)], fill='#1a4d1a', width=2)

    # 2. SCREEN
    if progress > 0.2:
        screen_prog = min(1.0, (progress - 0.2) / 0.3)
        screen_y_offset = (1.0 - screen_prog) * -400
        # Screen Base
        draw.rounded_rectangle(
            [(center_x - calc_w//2 + 30, center_y - calc_h//2 + 40 + screen_y_offset), 
             (center_x + calc_w//2 - 30, center_y - 80 + screen_y_offset)],
            radius=10, fill='#bdff00', outline='#111'
        )
        # Graph rendering fake
        if screen_prog > 0.8:
            pts = []
            for px in range(0, 340, 10):
                py = math.sin(px/40.0 + progress*10) * 80
                pts.append((center_x - 170 + px, center_y - 200 + py))
            draw.line(pts, fill='#000', width=4)

    # 3. OUTER CASE
    if progress > 0.5:
        case_prog = min(1.0, (progress - 0.5) / 0.2)
        case_alpha = int(case_prog * 255)
        # A deep obsidian cover
        draw.rounded_rectangle(
            [(center_x - calc_w//2 - 10, center_y - calc_h//2 - 10), 
             (center_x + calc_w//2 + 10, center_y + calc_h//2 + 10)],
            radius=25, outline=f'#333', width=10
        )
        # Masking the PCB except the screen
        draw.rounded_rectangle(
            [(center_x - calc_w//2, center_y - 80), 
             (center_x + calc_w//2, center_y + calc_h//2)],
            radius=0, fill='#111'
        )

    # 4. KEYS & ACCENTS
    if progress > 0.7:
        key_prog = min(1.0, (progress - 0.7) / 0.3)
        key_y = center_y - 20
        key_size = 40
        key_spacing = 60
        # Draw grid of keys
        for row in range(5):
            for col in range(4):
                kx = center_x - 100 + col * key_spacing
                ky = key_y + row * key_spacing
                draw.rounded_rectangle(
                    [(kx - key_size//2, ky - key_size//2),
                     (kx + key_size//2, ky + key_size//2)],
                    radius=5, fill='#222', outline='#00D1FF' if row == 0 else '#444'
                )

    # Save
    filename = f"{frames_dir}/frame_{str(i+1).zfill(3)}.webp"
    img.save(filename, "webp")
    print(f"Generated {filename}")

print("All 120 mockup frames generated successfully.")
