"""Writes Opposing Force's and Blue Shift's animated main menu titles: Half-Life's 25th anniversary title (liblist.gam's
animated_title "1", drawn and animated by GameUI.dll from resource/logo*.tga) remade with each game's name, in the
font of its menu buttons and its colours, into config/expansions/<game>/resource, installed over config/mod's.

Half-Life's ten pictures, matched picture for picture (measured from Valve's):
  logo.tga                800x86, the name in white, its letters spread from x 13 to 787, 45 tall from y 19;
  logo_blip.tga           the same blurred (sigma 4.75) and 1.67 times as opaque, a flash over it;
  logo_blip2.tga          the same thickened 5 pixels and blurred (sigma 2.75), a stronger flash;
  logo_big_blurred_N.tga  the name about twice as big and bolder, blurred, at most 53% opaque, drifting
                          behind it: one 1800x270 strip in four pieces the engine lays side by side from 320 pixels
                          left of the title (each piece flashes on its own, so each cut falls between letters);
  logo_big_blurred_blip_N the same pieces at most 97% opaque, their flashes.
Every picture is one colour with only its opacity varying, as Valve's are white.

Usage:
  python make_menu_titles.py
"""

import os

from PIL import Image, ImageDraw, ImageFilter, ImageFont

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

TITLES = {
    # Its logo's gold: the name light, the big one behind it deeper.
    "op4": ("OPPOSING FORCE", "resource/fonts/StardosStencil-Bold.ttf", (255, 222, 150), (232, 164, 64)),
    # Its logo's glowing blue: the name ice blue, the big one behind it the logo's blue.
    "bs": ("BLUE SHIFT", "resource/fonts/CourierPrime-Bold.ttf", (205, 225, 255), (80, 140, 255)),
}

SMALL = (800, 86)
SMALL_SPAN = (13, 787)      # the name's first and last pixels
SMALL_CAP = (19, 64)        # its capitals' top and baseline
BIG = (1800, 270)
BIG_SCALE, BIG_OFFSET = 2.05, 100   # the big name's letters where the small one's are, times 2.05, plus 100
BIG_CAP = (90, 177)
BIG_BOLDER = 3              # pixels each side: Valve's big letters are bolder than the small ones
PIECES = (466, 394, 507, 433)       # Valve's widths: the cuts are moved to the nearest gap
OVERSAMPLE = 4


def spread(text, font, left, right):
    """Each letter's centre (in OVERSAMPLE pixels), spread evenly so the ink runs from left to right, a space a gap
    wider."""
    o = OVERSAMPLE
    glyphs = [c for c in text if c != " "]
    inks = [font.getbbox(c)[2] - font.getbbox(c)[0] for c in glyphs]
    gap = ((right - left) * o - sum(inks)) / (len(glyphs) - 1 + text.count(" "))
    centres, x, i = [], left * o, 0
    for c in text:
        if c == " ":
            x += gap
            continue
        centres.append(x + inks[i] / 2)
        x += inks[i] + gap
        i += 1
    return centres


def letters(text, font_file, cap_top, baseline, size, centres=None):
    """The name's opacity at capitals from cap_top to baseline, rendered OVERSAMPLE times larger and reduced: its
    letters spread across SMALL_SPAN, or centred on the given centres (OVERSAMPLE pixels). Returns it and the
    centres."""
    o = OVERSAMPLE
    probe = ImageFont.truetype(font_file, 400)
    cap = probe.getbbox("H")[3] - probe.getbbox("H")[1]
    font = ImageFont.truetype(font_file, round(400 * (baseline - cap_top) * o / cap))
    centres = centres or spread(text, font, *SMALL_SPAN)
    image = Image.new("L", (size[0] * o, size[1] * o), 0)
    draw = ImageDraw.Draw(image)
    for c, centre in zip([c for c in text if c != " "], centres):
        b = font.getbbox(c, anchor="ls")
        draw.text((centre - (b[0] + b[2]) / 2, baseline * o), c, font=font, fill=255, anchor="ls")
    return image.resize(size, Image.LANCZOS), centres


def blur(alpha, sigma, gain, grow=0):
    if grow:
        alpha = alpha.filter(ImageFilter.MaxFilter(2 * grow + 1))
    alpha = alpha.filter(ImageFilter.GaussianBlur(sigma))
    return alpha.point(lambda v: min(255, round(v * gain)))


def save(path, color, alpha):
    image = Image.new("RGBA", alpha.size, color + (0,))
    image.putalpha(alpha)
    image.save(path)


def cuts(alpha):
    """Where to cut the big strip: near each of Valve's cuts, at the faintest column within 90 pixels."""
    columns = list(alpha.resize((alpha.width, 1), Image.BOX).tobytes())
    result, at = [0], 0
    for width in PIECES[:-1]:
        at += width
        result.append(min(range(at - 90, at + 91), key=lambda x: columns[x]))
    return result + [alpha.width]


def write(game, text, font_file, color, big_color):
    resource = os.path.join(REPO, "config", "expansions", game, "resource")
    font_file = os.path.join(REPO, "config", "expansions", game, font_file)

    small, centres = letters(text, font_file, *SMALL_CAP, SMALL)
    save(os.path.join(resource, "logo.tga"), color, small)
    save(os.path.join(resource, "logo_blip.tga"), color, blur(small, 4.75, 1.67))
    save(os.path.join(resource, "logo_blip2.tga"), color, blur(small, 2.75, 1.02, grow=5))

    # The big name: each letter where the small one's is, scaled, at the big capitals' height, then made bolder.
    big, _ = letters(text, font_file, *BIG_CAP, BIG, [c * BIG_SCALE + BIG_OFFSET * OVERSAMPLE for c in centres])
    big = big.filter(ImageFilter.MaxFilter(2 * BIG_BOLDER + 1))
    # Blurred more than Valve's (sigma about 6.5 on letters with thinner strokes) to look as soft, and as opaque at
    # the most as Valve's: 136, its flashes 248.
    soft = big.filter(ImageFilter.GaussianBlur(9))
    peak = soft.getextrema()[1]
    blurred = soft.point(lambda v: round(v * 136 / peak))
    blip = soft.point(lambda v: round(v * 248 / peak))
    edges = cuts(blurred)
    for n in range(4):
        box = (edges[n], 0, edges[n + 1], BIG[1])
        save(os.path.join(resource, f"logo_big_blurred_{n}.tga"), big_color, blurred.crop(box))
        save(os.path.join(resource, f"logo_big_blurred_blip_{n}.tga"), big_color, blip.crop(box))


def main():
    for game, title in TITLES.items():
        write(game, *title)
    print("make_menu_titles: Opposing Force's and Blue Shift's animated titles written")


if __name__ == "__main__":
    main()
