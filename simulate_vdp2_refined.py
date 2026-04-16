import numpy as np
from PIL import Image
import struct

def simulate_vdp2_true(pixels_h_path, palette_h_path, output_path, sx=0, sy=0, dx=0x0100, dyst=0x0100):
    # This simulator will use the binary logic of VDP2
    # sx, sy: 32-bit fixed point (16.16)
    # dx, dyst: 16-bit fixed point (8.8 for this test)
    
    # We'll mock the asset reading since we can't easily parse .h files
    # I'll use the .png as source but convert it to indexed 8-bit first
    tex_path = "examples/vdp2_infinite_plan/assets/seamless_sea.png"
    tex = Image.open(tex_path).convert('P')
    palette = np.array(tex.getpalette()).reshape(-1, 3)
    pixels = np.array(tex)
    th, tw = pixels.shape

    sw, sh = 320, 224
    output = np.zeros((sh, sw, 3), dtype=np.uint8)

    # VDP2 Mode A simulation
    xst = sx << 16
    yst = sy << 16

    for y_scr in range(sh):
        curr_x = xst
        curr_y = yst
        for x_scr in range(sw):
            # Sampling with wrap-around
            # Coordinates are 16.16, we want integer part for pixel index
            tx = (curr_x >> 16) % tw
            ty = (curr_y >> 16) % th
            
            color_idx = pixels[ty, tx]
            output[y_scr, x_scr] = palette[color_idx]
            
            # Step X (16-bit dx is added to the 32-bit accumulator)
            # In VDP2 Mode A, dx is added to X only?
            # Actually, per-pixel increment is dX and dY.
            curr_x += (dx << 8) # Assuming 8.8 format
        
        # Line increment
        # In Mode A: Xst += dXst, Yst += dYst
        xst += 0
        yst += (dyst << 8)

    Image.fromarray(output).save(output_path)
    print(f"Refined simulation saved to {output_path}")

if __name__ == "__main__":
    simulate_vdp2_true(None, None, "vdp2_sim_refined.png")
