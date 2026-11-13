"""Writes Opposing Force's and Blue Shift's animated main menu titles.

Usage:
  python make_menu_titles.py
"""

import os

from PIL import Image, ImageDraw, ImageFilter, ImageFont

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

TITLES = {
    # Its logo's gold, the big name behind it greyer and fainter.
    "op4": ("OPPOSING FORCE", "OPPOSING FORCE", "resource/fonts/StardosStencil-Bold.ttf", (255, 222, 150),
            (214, 196, 166), 0.85),
    "bs": ("blue shift", "BLUE SHIFT", "resource/fonts/CourierPrime-Bold.ttf", (90, 160, 255), (180, 200, 230), 1.0),
}

SMALL = (800, 86)
SMALL_SPAN = (13, 787)
SMALL_CAP = (19, 64)        # its capitals' top and baseline
BIG = (1800, 270)
BIG_SCALE, BIG_OFFSET = 2.05, 100
BIG_CAP = (90, 177)
BIG_BOLDER = 3
PIECES = (466, 394, 507, 433)
OVERSAMPLE = 4


def spread(text, font, left, right):
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


def write(game, text, big_text, font_file, color, big_color, big_opacity):
    resource = os.path.join(REPO, "config", "expansions", game, "resource")
    font_file = os.path.join(REPO, "config", "expansions", game, font_file)

    small, centres = letters(text, font_file, *SMALL_CAP, SMALL)
    save(os.path.join(resource, "logo.tga"), color, small)
    save(os.path.join(resource, "logo_blip.tga"), color, blur(small, 4.75, 1.67))
    save(os.path.join(resource, "logo_blip2.tga"), color, blur(small, 2.75, 1.02, grow=5))

    big, _ = letters(big_text, font_file, *BIG_CAP, BIG, [c * BIG_SCALE + BIG_OFFSET * OVERSAMPLE for c in centres])
    big = big.filter(ImageFilter.MaxFilter(2 * BIG_BOLDER + 1))
    soft = big.filter(ImageFilter.GaussianBlur(9))
    peak = soft.getextrema()[1] / big_opacity
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
