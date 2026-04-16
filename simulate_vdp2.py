import numpy as np
from PIL import Image

def simulate_vdp2_rbg0(texture_path, output_path, sx=0, sz=0, dx=1.0, dyst=1.0, dxst=0, dy=0):
    # Load texture
    tex = Image.open(texture_path).convert('RGB')
    tex_data = np.array(tex)
    th, tw, _ = tex_data.shape

    # Screen size
    sw, sh = 320, 224
    output = np.zeros((sh, sw, 3), dtype=np.uint8)

    # Current Start Coordinates (Fixed point simulation)
    # sx, sz are in pixels
    cur_xst = float(sx)
    cur_yst = float(sz)

    for y_scr in range(sh):
        x = cur_xst
        z = cur_yst
        for x_scr in range(sw):
            # Sample coordinate (Wrap-around for infinite plane)
            tx = int(x) % tw
            ty = int(z) % th
            
            output[y_scr, x_scr] = tex_data[ty, tx]
            
            # Step along the scanline
            x += dx
            z += dy
        
        # Step to next scanline
        cur_xst += dxst
        cur_yst += dyst

    Image.fromarray(output).save(output_path)
    print(f"Saved simulated output to {output_path}")

if __name__ == "__main__":
    import os
    tex_path = "examples/vdp2_infinite_plan/assets/seamless_sea.png"
    if os.path.exists(tex_path):
        simulate_vdp2_rbg0(tex_path, "vdp2_sim_identity.png", dx=1.0, dyst=1.0)
    else:
        print(f"Texture not found at {tex_path}")
