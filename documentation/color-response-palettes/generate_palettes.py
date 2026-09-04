#!/usr/bin/env python3
"""Generate deterministic MAGI-80 RGB12 color-response comparison grids.

The transforms are compact, inspectable working approximations for design review.
They deliberately model only a static color response; grain, halation, scanlines,
chroma delay, noise, and other spatial or temporal effects are out of scope.

This is a host-side fitting and reference tool and may use floating point. The Amiga
must consume a quantized LUT through integer-only direct indexing; Phase 0 may either
decode a packed canonical LUT or reconstruct it once from a byte-identical fixed-point
descriptor. It must not execute these floating-point transforms.
"""

from __future__ import annotations

import math
import struct
import zlib
from pathlib import Path
from typing import Callable, Iterable


RGB = tuple[float, float, float]
Transform = Callable[[RGB], RGB]

OUTPUT_DIR = Path(__file__).resolve().parent
CELL = 32
GAP = 4
LEVELS_RG = (0, 2, 4, 6, 9, 11, 13, 15)
LEVELS_B = (0, 5, 10, 15)


def clamp01(value: float) -> float:
    return min(1.0, max(0.0, value))


def srgb_to_linear(value: float) -> float:
    if value <= 0.04045:
        return value / 12.92
    return ((value + 0.055) / 1.055) ** 2.4


def linear_to_srgb(value: float) -> float:
    value = clamp01(value)
    if value <= 0.0031308:
        return 12.92 * value
    return 1.055 * (value ** (1.0 / 2.4)) - 0.055


def mat_vec(matrix: tuple[tuple[float, float, float], ...], rgb: RGB) -> RGB:
    return tuple(sum(row[index] * rgb[index] for index in range(3)) for row in matrix)  # type: ignore[return-value]


def mix(a: RGB, b: RGB, amount: float) -> RGB:
    return tuple(a[index] * (1.0 - amount) + b[index] * amount for index in range(3))  # type: ignore[return-value]


def luminance(rgb: RGB) -> float:
    return 0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2]


def adjust_saturation(rgb: RGB, amount: float) -> RGB:
    grey = (luminance(rgb),) * 3
    return mix(grey, rgb, amount)


def power_sigmoid(value: float, contrast: float) -> float:
    value = clamp01(value)
    if value <= 0.0 or value >= 1.0:
        return value
    left = value**contrast
    right = (1.0 - value) ** contrast
    return left / (left + right)


def film_pipeline(
    rgb: RGB,
    *,
    matrix: tuple[tuple[float, float, float], ...],
    saturation: float,
    contrast: float,
    black: RGB,
    white: RGB,
    exposure: float = 1.0,
    shadow_tint: RGB | None = None,
    highlight_tint: RGB | None = None,
) -> RGB:
    linear = tuple(srgb_to_linear(channel) * exposure for channel in rgb)
    linear = mat_vec(matrix, linear)
    linear = adjust_saturation(linear, saturation)
    toned = tuple(power_sigmoid(clamp01(channel), contrast) for channel in linear)

    if shadow_tint is not None:
        shadow_weight = (1.0 - clamp01(luminance(toned))) ** 2
        toned = tuple(
            channel + shadow_tint[index] * shadow_weight
            for index, channel in enumerate(toned)
        )
    if highlight_tint is not None:
        highlight_weight = clamp01(luminance(toned)) ** 2
        toned = tuple(
            channel + highlight_tint[index] * highlight_weight
            for index, channel in enumerate(toned)
        )

    mapped = tuple(
        black[index] + clamp01(toned[index]) * (white[index] - black[index])
        for index in range(3)
    )
    return tuple(linear_to_srgb(channel) for channel in mapped)  # type: ignore[return-value]


def vanilla(rgb: RGB) -> RGB:
    return rgb


def portra_400(rgb: RGB) -> RGB:
    return film_pipeline(
        rgb,
        matrix=((1.035, 0.010, -0.020), (0.010, 0.995, 0.005), (-0.015, 0.025, 0.940)),
        saturation=0.90,
        contrast=0.88,
        black=(0.010, 0.008, 0.006),
        white=(0.985, 0.970, 0.935),
        exposure=1.03,
        shadow_tint=(0.006, 0.002, -0.006),
        highlight_tint=(0.010, 0.004, -0.008),
    )


def ektachrome_e100(rgb: RGB) -> RGB:
    return film_pipeline(
        rgb,
        matrix=((0.995, -0.005, 0.010), (-0.005, 1.015, 0.010), (-0.010, 0.015, 1.055)),
        saturation=1.12,
        contrast=1.18,
        black=(0.002, 0.003, 0.006),
        white=(0.975, 0.990, 1.000),
        exposure=1.01,
        shadow_tint=(-0.004, 0.002, 0.010),
    )


def polaroid_600(rgb: RGB) -> RGB:
    return film_pipeline(
        rgb,
        matrix=((1.040, 0.035, -0.035), (0.020, 0.965, 0.015), (0.010, 0.045, 0.855)),
        saturation=0.82,
        contrast=0.84,
        black=(0.030, 0.024, 0.018),
        white=(0.930, 0.900, 0.825),
        exposure=1.04,
        shadow_tint=(0.012, 0.004, -0.012),
        highlight_tint=(0.020, 0.012, -0.018),
    )


def lomochrome_metropolis(rgb: RGB) -> RGB:
    return film_pipeline(
        rgb,
        matrix=((0.930, 0.075, -0.005), (0.025, 0.995, 0.055), (0.015, 0.080, 0.800)),
        saturation=0.58,
        contrast=1.20,
        black=(0.018, 0.026, 0.025),
        white=(0.905, 0.920, 0.825),
        exposure=0.98,
        shadow_tint=(-0.008, 0.014, 0.018),
        highlight_tint=(0.014, 0.010, -0.014),
    )


def ilford_hp5_plus(rgb: RGB) -> RGB:
    linear = tuple(srgb_to_linear(channel) for channel in rgb)
    # Panchromatic weighting approximates the published HP5 PLUS sensitivity
    # envelope: broad green response, meaningful red reach, and lower blue weight.
    grey = 0.26 * linear[0] + 0.63 * linear[1] + 0.11 * linear[2]
    grey = 0.012 + 0.950 * power_sigmoid(grey, 1.12)
    encoded = linear_to_srgb(grey)
    return encoded, encoded, encoded


MACHADO_DEUTERANOPIA_2009 = (
    (0.367322, 0.860646, -0.227968),
    (0.280085, 0.672501, 0.047413),
    (-0.011820, 0.042940, 0.968881),
)
MACHADO_PROTANOPIA_2009 = (
    (0.152286, 1.052583, -0.204868),
    (0.114503, 0.786281, 0.099216),
    (-0.003882, -0.048116, 1.051998),
)


def simulate_color_vision(rgb: RGB, matrix: tuple[tuple[float, float, float], ...]) -> RGB:
    linear = tuple(srgb_to_linear(channel) for channel in rgb)
    simulated = mat_vec(matrix, linear)
    return tuple(linear_to_srgb(channel) for channel in simulated)  # type: ignore[return-value]


def deutan_vision_2009(rgb: RGB) -> RGB:
    return simulate_color_vision(rgb, MACHADO_DEUTERANOPIA_2009)


def protan_vision_2009(rgb: RGB) -> RGB:
    return simulate_color_vision(rgb, MACHADO_PROTANOPIA_2009)


def megadrive_1988(rgb: RGB) -> RGB:
    # Pull colors only part-way toward the console's 3-bit-per-channel RGB
    # vocabulary. This evokes its stepped palette without collapsing MAGI-80
    # from 4,096 logical inputs to 512 colors. The purple offset is a MAGI-80
    # art-direction choice, not a claim about the VDP DAC: a luminance bell
    # confines it to mid-tones and makes it vanish at black and white.
    quantized = tuple(round(channel * 7.0) / 7.0 for channel in rgb)
    softly_quantized = tuple(
        0.72 * rgb[index] + 0.28 * quantized[index] for index in range(3)
    )
    linear = tuple(srgb_to_linear(channel) for channel in softly_quantized)
    y = clamp01(luminance(linear))
    midtone = math.sin(math.pi * y) ** 2
    biased = (
        linear[0] + (1.0 - linear[0]) * 0.050 * midtone,
        linear[1] * (1.0 - 0.040 * midtone),
        linear[2] + (1.0 - linear[2]) * 0.070 * midtone,
    )
    return tuple(linear_to_srgb(channel) for channel in biased)  # type: ignore[return-value]


def xy_to_xyz(x: float, y: float) -> RGB:
    return x / y, 1.0, (1.0 - x - y) / y


def inverse_3x3(matrix: tuple[tuple[float, float, float], ...]) -> tuple[tuple[float, float, float], ...]:
    a, b, c = matrix[0]
    d, e, f = matrix[1]
    g, h, i = matrix[2]
    det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g)
    return (
        ((e * i - f * h) / det, (c * h - b * i) / det, (b * f - c * e) / det),
        ((f * g - d * i) / det, (a * i - c * g) / det, (c * d - a * f) / det),
        ((d * h - e * g) / det, (b * g - a * h) / det, (a * e - b * d) / det),
    )


def rgb_to_xyz_matrix(
    primaries: tuple[tuple[float, float], tuple[float, float], tuple[float, float]],
    white: tuple[float, float],
) -> tuple[tuple[float, float, float], ...]:
    columns = tuple(xy_to_xyz(*primary) for primary in primaries)
    basis = tuple(tuple(columns[column][row] for column in range(3)) for row in range(3))
    scale = mat_vec(inverse_3x3(basis), xy_to_xyz(*white))
    return tuple(
        tuple(basis[row][column] * scale[column] for column in range(3))
        for row in range(3)
    )


BRADFORD = (
    (0.8951, 0.2664, -0.1614),
    (-0.7502, 1.7135, 0.0367),
    (0.0389, -0.0685, 1.0296),
)
BRADFORD_INV = inverse_3x3(BRADFORD)
XYZ_TO_SRGB = (
    (3.2404542, -1.5371385, -0.4985314),
    (-0.9692660, 1.8760108, 0.0415560),
    (0.0556434, -0.2040259, 1.0572252),
)
D65 = (0.3127, 0.3290)
ILLUMINANT_C = (0.310, 0.316)
NTSC_1953_PRIMARIES = ((0.67, 0.33), (0.21, 0.71), (0.14, 0.08))
PAL_SECAM_PRIMARIES = ((0.64, 0.33), (0.29, 0.60), (0.15, 0.06))


def adapt_xyz(xyz: RGB, source_white: tuple[float, float]) -> RGB:
    source_cone = mat_vec(BRADFORD, xy_to_xyz(*source_white))
    target_cone = mat_vec(BRADFORD, xy_to_xyz(*D65))
    cone = mat_vec(BRADFORD, xyz)
    adapted = tuple(cone[index] * target_cone[index] / source_cone[index] for index in range(3))
    return mat_vec(BRADFORD_INV, adapted)  # type: ignore[arg-type]


def historical_video(
    rgb: RGB,
    *,
    primaries: tuple[tuple[float, float], tuple[float, float], tuple[float, float]],
    white: tuple[float, float],
    gamma: float,
    chroma_uv: tuple[float, float] = (1.0, 1.0),
) -> RGB:
    red, green, blue = rgb
    if chroma_uv != (1.0, 1.0):
        y = 0.299 * red + 0.587 * green + 0.114 * blue
        u = (blue - y) * chroma_uv[0]
        v = (red - y) * chroma_uv[1]
        blue = y + u
        red = y + v
        green = (y - 0.299 * red - 0.114 * blue) / 0.587
    drive = tuple(clamp01(channel) ** gamma for channel in (red, green, blue))
    xyz = mat_vec(rgb_to_xyz_matrix(primaries, white), drive)
    xyz = adapt_xyz(xyz, white)
    display_linear = mat_vec(XYZ_TO_SRGB, xyz)
    return tuple(linear_to_srgb(channel) for channel in display_linear)  # type: ignore[return-value]


def ntsc_1953(rgb: RGB) -> RGB:
    return historical_video(
        rgb,
        primaries=NTSC_1953_PRIMARIES,
        white=ILLUMINANT_C,
        gamma=2.2,
    )


def pal_secam_625(rgb: RGB) -> RGB:
    return historical_video(
        rgb,
        primaries=PAL_SECAM_PRIMARIES,
        white=D65,
        gamma=2.8,
    )


def oskm_1960(rgb: RGB) -> RGB:
    # OSKM was a Soviet 625/50 quadrature system derived from NTSC but using
    # U/V-style chroma. The chroma contraction is explicitly a reconstruction
    # assumption pending primary-source receiver measurements.
    return historical_video(
        rgb,
        primaries=NTSC_1953_PRIMARIES,
        white=ILLUMINANT_C,
        gamma=2.5,
        chroma_uv=(0.84, 0.90),
    )


PROFILES: tuple[tuple[str, Transform], ...] = (
    ("00-amiga-rgb12-vanilla-1985.png", vanilla),
    ("01-kodak-portra-400-2010.png", portra_400),
    ("02-kodak-ektachrome-e100-2018.png", ektachrome_e100),
    ("03-polaroid-color-600-1981.png", polaroid_600),
    ("04-lomochrome-metropolis-2019.png", lomochrome_metropolis),
    ("05-ilford-hp5-plus-1989.png", ilford_hp5_plus),
    ("06-ntsc-1953.png", ntsc_1953),
    ("07-pal-secam-625-1967.png", pal_secam_625),
    ("08-oskm-1960.png", oskm_1960),
    ("09-deutan-machado-2009.png", deutan_vision_2009),
    ("10-protan-machado-2009.png", protan_vision_2009),
    ("11-megadrive-1988.png", megadrive_1988),
)

PROFILES_REQUIRING_4096_DISTINCT_OUTPUTS = {
    "00-amiga-rgb12-vanilla-1985.png",
    "11-megadrive-1988.png",
}


def byte_rgb(rgb: RGB) -> tuple[int, int, int]:
    return tuple(int(round(clamp01(channel) * 255.0)) for channel in rgb)  # type: ignore[return-value]


def logical_rgb(index: int) -> RGB:
    return (
        ((index >> 8) & 0xF) / 15.0,
        ((index >> 4) & 0xF) / 15.0,
        (index & 0xF) / 15.0,
    )


def build_lut(transform: Transform) -> tuple[tuple[int, int, int], ...]:
    lut = tuple(byte_rgb(transform(logical_rgb(index))) for index in range(4096))
    assert len(lut) == 4096
    assert all(0 <= channel <= 255 for color in lut for channel in color)
    return lut


def png_chunk(chunk_type: bytes, data: bytes) -> bytes:
    return (
        struct.pack(">I", len(data))
        + chunk_type
        + data
        + struct.pack(">I", zlib.crc32(chunk_type + data) & 0xFFFFFFFF)
    )


def write_png(path: Path, width: int, height: int, pixels: Iterable[tuple[int, int, int]]) -> None:
    rows = bytearray()
    iterator = iter(pixels)
    for _ in range(height):
        rows.append(0)
        for _ in range(width):
            rows.extend(next(iterator))
    try:
        next(iterator)
    except StopIteration:
        pass
    else:
        raise ValueError("pixel iterator exceeds image dimensions")

    payload = b"\x89PNG\r\n\x1a\n"
    payload += png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    payload += png_chunk(b"sRGB", b"\x00")
    payload += png_chunk(b"gAMA", struct.pack(">I", 45455))
    payload += png_chunk(b"tEXt", b"Software\x00MAGI-80 deterministic palette generator")
    payload += png_chunk(b"IDAT", zlib.compress(bytes(rows), level=9))
    payload += png_chunk(b"IEND", b"")
    path.write_bytes(payload)


def render_grid(lut: tuple[tuple[int, int, int], ...]) -> tuple[int, int, list[tuple[int, int, int]]]:
    width = 16 * CELL + GAP
    height = 16 * CELL + GAP
    background = (24, 24, 24)
    pixels = [background] * (width * height)

    for blue_slice, blue in enumerate(LEVELS_B):
        quadrant_x = blue_slice % 2
        quadrant_y = blue_slice // 2
        origin_x = quadrant_x * (8 * CELL + GAP)
        origin_y = quadrant_y * (8 * CELL + GAP)
        for green_index, green in enumerate(LEVELS_RG):
            for red_index, red in enumerate(LEVELS_RG):
                color = lut[(red << 8) | (green << 4) | blue]
                left = origin_x + red_index * CELL
                top = origin_y + green_index * CELL
                for y in range(top, top + CELL):
                    row = y * width
                    for x in range(left, left + CELL):
                        pixels[row + x] = color
    return width, height, pixels


def main() -> None:
    for filename, transform in PROFILES:
        lut = build_lut(transform)
        distinct = len(set(lut))
        if filename in PROFILES_REQUIRING_4096_DISTINCT_OUTPUTS:
            assert distinct == 4096, f"{filename} collapsed the logical RGB12 gamut"
        if filename == "11-megadrive-1988.png":
            assert lut[0x000] == (0, 0, 0)
            assert lut[0xFFF] == (255, 255, 255)
        width, height, pixels = render_grid(lut)
        write_png(OUTPUT_DIR / filename, width, height, pixels)
        print(f"generated {filename}: {width}x{height}, {distinct} distinct LUT colors")


if __name__ == "__main__":
    main()
