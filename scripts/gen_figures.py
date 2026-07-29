#!/usr/bin/env python3
"""Generate the Intel TBB guide's SVG figures, tuned to the htmler blue theme.

Same house style as the other know-how guides: because the figures are inlined
as static images (no page CSS reaches them), every colour is chosen to work on
BOTH the dark (#0b0d12) and light (#ffffff) themes at once. A mid-slate around
luminance ~0.2 gives roughly 4.3:1 contrast three ways -- white text on the
fill, and the same colour as ink on either background.

  * slate blue  #6B7B94  (neutral boxes, connectors, axes, labels)
  * blue        #3E7CC0  (highlighted / worker-thread boxes)       + dark #2F5F98
  * teal        #1F918C  (positive "result" accent / tasks)
  * amber       #D9922B  (warning / contention; dark text on fill)
  * red         #D65A5F  (problem callouts / errors)
  * muted       #9AA0B4  (captions)
  * white       #FFFFFF  (text inside dark fills)
  * 1.5pt wide rules, hand-drawn Virgil font stack

Run:  python3 scripts/gen_figures.py
Output: figures/*.svg  (referenced from the chapter markdown)
"""
import base64
import io
import math
import os

# House-style constants (htmler blue theme, dual light/dark legible)
GREY = "#6B7B94"
GREY_D = "#55637A"
BLUE = "#3E7CC0"
BLUE_D = "#2F5F98"
TEAL = "#1F918C"
AMBER = "#D9922B"
RED = "#D65A5F"
WHITE = "#FFFFFF"
LIGHT = "#9AA0B4"
INK_DARK = "#1F2433"
FONT = ("'Virgil','Segoe Print','Bradley Hand','Comic Sans MS',"
        "'Segoe UI',system-ui,-apple-system,sans-serif")
MONO = ("'Virgil','SFMono-Regular',ui-monospace,'JetBrains Mono',Consolas,"
        "monospace")
RULE = 1.5

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         "Virgil.woff2")

USED_CHARS = set()
FONT_STYLE = ""


def esc(s):
    USED_CHARS.update(str(s))
    return (str(s).replace("&", "&amp;").replace("<", "&lt;")
            .replace(">", "&gt;"))


def defs():
    marks = []
    for name, col in (("g", GREY), ("p", BLUE), ("t", TEAL),
                      ("r", RED), ("a", AMBER), ("l", LIGHT)):
        marks.append(
            f'<marker id="ah-{name}" viewBox="0 0 10 10" refX="8" refY="5" '
            f'markerWidth="4.5" markerHeight="4.5" '
            f'orient="auto-start-reverse">'
            f'<path d="M0 1L9 5L0 9z" fill="{col}"/></marker>')
    return "<defs>" + "".join(marks) + "</defs>"


def rrect(x, y, w, h, fill, rx=9, stroke=None, sw=RULE, dash=None, opacity=None):
    s = (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" ry="{rx}" '
         f'fill="{fill}"')
    if stroke:
        s += f' stroke="{stroke}" stroke-width="{sw}"'
    if dash:
        s += f' stroke-dasharray="{dash}"'
    if opacity is not None:
        s += f' opacity="{opacity}"'
    return s + "/>"


def tspan_lines(x, cy, lines, fill, size, weight, lh, mono=False):
    fam = MONO if mono else FONT
    n = len(lines)
    y0 = cy - (n - 1) * lh / 2.0
    out = [f'<text x="{x}" y="{y0}" fill="{fill}" font-family="{fam}" '
           f'font-size="{size}" font-weight="{weight}" text-anchor="middle" '
           f'dominant-baseline="central">']
    for i, ln in enumerate(lines):
        dy = 0 if i == 0 else lh
        out.append(f'<tspan x="{x}" dy="{dy}">{esc(ln)}</tspan>')
    out.append("</text>")
    return "".join(out)


def box(x, y, w, h, lines, fill=GREY, tcol=WHITE, size=13, weight=600,
        rx=9, lh=16, stroke=None, sw=RULE, dash=None, mono=False):
    if isinstance(lines, str):
        lines = lines.split("\n")
    r = rrect(x, y, w, h, fill, rx=rx, stroke=stroke, sw=sw, dash=dash)
    t = tspan_lines(x + w / 2.0, y + h / 2.0, lines, tcol, size, weight, lh, mono)
    return r + t


def obox(x, y, w, h, lines, stroke=GREY, tcol=GREY, size=13, weight=600,
         rx=9, lh=16, sw=RULE, dash=None, fill="none", mono=False):
    r = rrect(x, y, w, h, fill, rx=rx, stroke=stroke, sw=sw, dash=dash)
    t = tspan_lines(x + w / 2.0, y + h / 2.0, lines if isinstance(lines, list)
                    else [lines], tcol, size, weight, lh, mono)
    return r + t


def text(x, y, s, fill=GREY, size=13, weight=600, anchor="middle",
         italic=False, mono=False):
    fam = MONO if mono else FONT
    return (f'<text x="{x}" y="{y}" fill="{fill}" font-family="{fam}" '
            f'font-size="{size}" font-weight="{weight}" text-anchor="{anchor}"'
            f' dominant-baseline="central">{esc(s)}</text>')


def line(x1, y1, x2, y2, col=GREY, sw=RULE, dash=None):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    return (f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{col}" '
            f'stroke-width="{sw}"{d}/>')


def _mk(col):
    return {GREY: "g", BLUE: "p", TEAL: "t", RED: "r", AMBER: "a",
            LIGHT: "l"}.get(col, "g")


def arrow(x1, y1, x2, y2, col=GREY, sw=RULE, dash=None):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    return (f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{col}" '
            f'stroke-width="{sw}" marker-end="url(#ah-{_mk(col)})"{d}/>')


def path(d, col=GREY, sw=RULE, dash=None, arrow_end=False, fill="none"):
    dd = f' stroke-dasharray="{dash}"' if dash else ""
    m = f' marker-end="url(#ah-{_mk(col)})"' if arrow_end else ""
    return (f'<path d="{d}" fill="{fill}" stroke="{col}" stroke-width="{sw}"'
            f'{dd}{m}/>')


def circle(cx, cy, r, fill, stroke=None, sw=RULE):
    st = f' stroke="{stroke}" stroke-width="{sw}"' if stroke else ""
    return f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="{fill}"{st}/>'


def cylinder(x, y, w, h, fill=GREY, tcol=WHITE, lines=None, size=12,
             stroke=None, sw=RULE):
    ry = min(h * 0.16, 14)
    st = (f' stroke="{stroke}" stroke-width="{sw}"') if stroke else ""
    body = (f'<path d="M{x} {y+ry} A{w/2} {ry} 0 0 0 {x+w} {y+ry} '
            f'L{x+w} {y+h-ry} A{w/2} {ry} 0 0 1 {x} {y+h-ry} Z" '
            f'fill="{fill}"{st}/>')
    top = (f'<ellipse cx="{x+w/2}" cy="{y+ry}" rx="{w/2}" ry="{ry}" '
           f'fill="{fill}"{st}/>')
    t = ""
    if lines:
        t = tspan_lines(x + w / 2.0, y + h / 2.0 + ry / 2, lines, tcol, size,
                        600, 15)
    return body + top + t


def dash_boundary(x1, y, x2, label=None):
    """The user/kernel privilege boundary: a double dashed rule."""
    out = [line(x1, y, x2, y, AMBER, 1.4, dash="7 5"),
           line(x1, y + 4, x2, y + 4, AMBER, 1.4, dash="7 5")]
    if label:
        out.append(text((x1 + x2) / 2, y - 10, label, AMBER, 11, 700))
    return "".join(out)


def svg(w, h, body, title=""):
    t = f"<title>{esc(title)}</title>" if title else ""
    return (f'<?xml version="1.0" encoding="UTF-8"?>\n'
            f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" '
            f'width="{w}" height="{h}" font-family="{FONT}">{t}{FONT_STYLE}'
            f'{defs()}{body}</svg>\n')


def write(rel_path, content):
    full = os.path.join(REPO_ROOT, rel_path)
    os.makedirs(os.path.dirname(full), exist_ok=True)
    with open(full, "w") as f:
        f.write(content)
    print("wrote", rel_path, f"({len(content)} bytes)")


# FIGURES

ALL = []


def fig(fn):
    ALL.append(fn)
    return fn


# -- 00 foundations ----------------------------------------------------------
@fig
def fig_tbb_stack():
    W, H = 760, 420
    b = [text(W / 2, 26, "The oneTBB layer cake", GREY, 16, 700)]
    layers = [
        ("Parallel algorithms  \u00b7  Flow Graph  \u00b7  Concurrent containers",
         BLUE, 60),
        ("Generic parallel patterns (parallel_for / reduce / pipeline)", BLUE_D,
         116),
        ("Task scheduler  \u2014  work-stealing, arenas, tasks", TEAL, 172),
        ("Scalable memory allocator (tbbmalloc)", GREY, 228),
        ("OS threads  \u00b7  hardware cores", GREY_D, 284),
    ]
    for lab, col, y in layers:
        b.append(box(80, y, 600, 44, lab, col, size=12, rx=10))
    for y in (104, 160, 216, 272):
        b.append(arrow(W / 2, y, W / 2, y + 12, LIGHT))
    b.append(text(W / 2, 356, "you write WHAT to compute; the scheduler decides "
                  "WHEN and on WHICH thread", LIGHT, 12, 500))
    b.append(text(W / 2, H - 18, "everything above the task scheduler is "
                  "composable \u2014 nest algorithms freely", LIGHT, 11, 500))
    write("figures/tbb-stack.svg", svg(W, H, "".join(b), "TBB stack"))


@fig
def fig_task_vs_thread():
    W, H = 780, 400
    b = [text(W / 2, 26, "Many tasks map onto few worker threads", GREY, 16,
              700)]
    b.append(text(190, 60, "your logical work", TEAL, 12, 700))
    for i in range(12):
        r, c = divmod(i, 4)
        b.append(box(70 + c * 70, 80 + r * 46, 56, 34, "task", TEAL, size=10,
                     rx=6))
    b.append(text(600, 60, "worker pool", BLUE, 12, 700))
    for i in range(4):
        b.append(box(520, 80 + i * 62, 200, 46, [f"worker {i}", "= 1 OS thread "
                     "pinned to a core"], BLUE, size=10, lh=14, rx=8))
    for i in range(4):
        b.append(arrow(354, 150, 520, 103 + i * 62, GREY, dash="4 4"))
    b.append(text(W / 2, 330, "TBB sizes the pool to hardware_concurrency() by "
                  "default \u2014 one worker per logical core", LIGHT, 11, 500))
    b.append(text(W / 2, H - 22, "tasks are ~cheap function objects; threads are "
                  "expensive OS resources. Never map 1 task = 1 thread.", LIGHT,
                  11, 500))
    write("figures/task-vs-thread.svg", svg(W, H, "".join(b),
          "Tasks vs threads"))


@fig
def fig_work_stealing():
    W, H = 820, 420
    b = [text(W / 2, 26, "Work-stealing scheduler", GREY, 16, 700)]
    for i, x in enumerate((60, 260, 460)):
        b.append(box(x, 70, 150, 30, f"worker {i}", BLUE, size=11, rx=8))
        for j in range(3):
            fill = TEAL if not (i == 2 and j > 0) else "none"
            if i == 2 and j > 0:
                b.append(obox(x + 20, 112 + j * 44, 110, 34, "", GREY, GREY,
                              rx=6, dash="4 3"))
            else:
                b.append(box(x + 20, 112 + j * 44, 110, 34, "task", TEAL,
                             size=10, rx=6))
    b.append(box(640, 70, 150, 30, "worker 3 (idle)", AMBER, size=10, rx=8))
    b.append(text(715, 130, "empty deque", LIGHT, 10, 500))
    b.append(text(715, 148, "\u2192 steal!", AMBER, 11, 700))
    b.append(arrow(700, 168, 480, 130, AMBER))
    b.append(text(135, 258, "push/pop LIFO", LIGHT, 9, 500))
    b.append(text(135, 274, "(own end)", LIGHT, 9, 500))
    b.append(text(W / 2, 300, "a thief steals FIFO from the OLD end \u2014 the "
                  "biggest, least-recently-split task", LIGHT, 11, 500))
    b.append(text(W / 2, 350, "each worker owns a double-ended queue (deque) of "
                  "ready tasks", GREY, 12, 600))
    b.append(text(W / 2, H - 20, "local LIFO keeps data hot in cache; stealing "
                  "the oldest task grabs the most work \u2192 balanced with "
                  "little contention", LIGHT, 11, 500))
    write("figures/work-stealing.svg", svg(W, H, "".join(b), "Work stealing"))


@fig
def fig_recursive_splitting():
    W, H = 800, 400
    b = [text(W / 2, 26, "Recursive range splitting", GREY, 15, 700)]
    b.append(box(320, 56, 160, 34, "[0, 1000)", BLUE, size=11, rx=8, mono=True))
    b.append(box(180, 116, 150, 32, "[0, 500)", BLUE, size=10, rx=8, mono=True))
    b.append(box(470, 116, 150, 32, "[500, 1000)", BLUE, size=10, rx=8,
                 mono=True))
    leaves = ["[0,250)", "[250,500)", "[500,750)", "[750,1000)"]
    for i, lv in enumerate(leaves):
        b.append(box(60 + i * 185, 186, 150, 32, lv, TEAL, size=10, rx=8,
                     mono=True))
        b.append(box(60 + i * 185, 250, 150, 30, "run on a worker", TEAL,
                     size=9, rx=6))
        b.append(arrow(135 + i * 185, 218, 135 + i * 185, 250, GREY))
    b.append(arrow(370, 90, 255, 116, GREY))
    b.append(arrow(430, 90, 545, 116, GREY))
    b.append(arrow(230, 148, 135, 186, GREY))
    b.append(arrow(280, 148, 320, 186, GREY))
    b.append(arrow(520, 148, 505, 186, GREY))
    b.append(arrow(570, 148, 690, 186, GREY))
    b.append(text(W / 2, 320, "split() keeps halving a blocked_range while "
                  "is_divisible() is true (size > grainsize)", LIGHT, 11, 500))
    b.append(text(W / 2, H - 18, "the split tree IS the task tree \u2014 "
                  "subranges become stealable tasks", LIGHT, 11, 500))
    write("figures/recursive-splitting.svg", svg(W, H, "".join(b),
          "Recursive splitting"))


@fig
def fig_grain_size():
    W, H = 820, 360
    b = [text(W / 2, 26, "Grain size: the overhead / balance trade-off", GREY,
              15, 700)]
    cases = [
        (60, "too fine", RED, "grainsize=1", "scheduling overhead swamps the "
         "real work", [10] * 12),
        (300, "tuned", TEAL, "grainsize \u2248 100", "overhead amortized, load "
         "still balanced", [40, 42, 38, 41]),
        (560, "too coarse", AMBER, "grainsize huge", "idle workers: not enough "
         "chunks to steal", [120, 20]),
    ]
    for x, name, col, sub, note, chunks in cases:
        tc = INK_DARK if col == AMBER else WHITE
        b.append(box(x, 60, 200, 30, name, col, tcol=tc, size=12, rx=8))
        b.append(text(x + 100, 104, sub, GREY, 10, 600, mono=True))
        y = 120
        for c in chunks:
            hgt = max(6, c / 4)
            b.append(rrect(x + 20, y, 160, hgt, col, rx=3))
            y += hgt + 3
        b.append(text(x + 100, 300, note, LIGHT, 9, 500))
    b.append(text(W / 2, H - 16, "rule of thumb: a chunk should run for at "
                  "least ~1\u201310 microseconds (\u2265 ~10k cycles)", LIGHT,
                  11, 500))
    write("figures/grain-size.svg", svg(W, H, "".join(b), "Grain size"))


@fig
def fig_partitioners():
    W, H = 800, 380
    b = [text(W / 2, 26, "Partitioners: how the range is chopped", GREY, 16,
              700)]
    rows = [
        ("auto_partitioner", "default; splits adaptively to feed idle workers",
         "most cases", TEAL),
        ("simple_partitioner", "split down to grainsize, always", "you tuned "
         "grainsize by hand", BLUE),
        ("static_partitioner", "one even chunk per worker, no stealing",
         "uniform work, low overhead", BLUE_D),
        ("affinity_partitioner", "replay the same mapping across calls",
         "cache reuse over iterations", GREY_D),
    ]
    for i, (name, how, when, col) in enumerate(rows):
        y = 64 + i * 74
        b.append(box(50, y, 230, 54, name, col, size=12, rx=10, mono=False))
        b.append(text(300, y + 20, how, GREY, 11, 500, anchor="start"))
        b.append(text(300, y + 40, "use when: " + when, LIGHT, 10, 500,
                      anchor="start"))
    b.append(text(W / 2, H - 16, "start with auto_partitioner; only switch when "
                  "profiling shows imbalance or cache misses", LIGHT, 11, 500))
    write("figures/partitioners.svg", svg(W, H, "".join(b), "Partitioners"))


# -- 01 parallel algorithms --------------------------------------------------
@fig
def fig_parallel_for():
    W, H = 780, 340
    b = [text(W / 2, 26, "parallel_for: independent iterations, split range",
              GREY, 15, 700)]
    b.append(box(60, 70, 660, 34, "for i in [0, N):  a[i] = f(a[i])   (no "
                 "cross-iteration deps)", GREY_D, size=11, rx=8, mono=False))
    labels = ["[0, N/4)", "[N/4, N/2)", "[N/2, 3N/4)", "[3N/4, N)"]
    for i, lab in enumerate(labels):
        x = 60 + i * 168
        b.append(arrow(x + 75, 104, x + 75, 140, GREY))
        b.append(box(x, 140, 155, 44, lab, TEAL, size=10, rx=8, mono=True))
        b.append(box(x, 200, 155, 40, f"worker {i}", BLUE, size=10, rx=8))
        b.append(arrow(x + 77, 184, x + 77, 200, GREY))
    b.append(text(W / 2, 280, "each subrange runs on whichever worker grabs it; "
                  "order across chunks is NOT defined", LIGHT, 11, 500))
    b.append(text(W / 2, H - 16, "correctness rule: iterations must be "
                  "independent (or use a reduction / concurrent container)",
                  LIGHT, 11, 500))
    write("figures/parallel-for.svg", svg(W, H, "".join(b), "parallel_for"))


@fig
def fig_parallel_reduce():
    W, H = 800, 380
    b = [text(W / 2, 26, "parallel_reduce: per-chunk partials, then join", GREY,
              15, 700)]
    parts = ["sum[0,25)", "sum[25,50)", "sum[50,75)", "sum[75,100)"]
    for i, p in enumerate(parts):
        x = 60 + i * 185
        b.append(box(x, 70, 150, 40, p, TEAL, size=10, rx=8, mono=True))
    b.append(box(150, 170, 200, 40, "join: L + R", BLUE, size=11, rx=8))
    b.append(box(470, 170, 200, 40, "join: L + R", BLUE, size=11, rx=8))
    b.append(arrow(135, 110, 220, 170, GREY))
    b.append(arrow(320, 110, 280, 170, GREY))
    b.append(arrow(505, 110, 545, 170, GREY))
    b.append(arrow(690, 110, 600, 170, GREY))
    b.append(box(300, 270, 200, 44, "total sum", BLUE_D, size=12, rx=10))
    b.append(arrow(250, 210, 360, 270, GREY))
    b.append(arrow(570, 210, 440, 270, GREY))
    b.append(text(W / 2, H - 30, "you provide: (1) a body that folds a subrange, "
                  "(2) a join() that combines two partials", LIGHT, 11, 500))
    b.append(text(W / 2, H - 12, "join must be ASSOCIATIVE; result order may "
                  "vary \u2192 floating-point sums aren't bit-identical", LIGHT,
                  11, 500))
    write("figures/parallel-reduce.svg", svg(W, H, "".join(b),
          "parallel_reduce"))


@fig
def fig_parallel_scan():
    W, H = 800, 360
    b = [text(W / 2, 26, "parallel_scan: two sweeps for a prefix sum", GREY, 15,
              700)]
    b.append(text(W / 2, 58, "input:  3  1  4  1  5  9  2  6", GREY, 13, 600,
                  mono=True))
    b.append(box(80, 90, 300, 60, ["1. pre-scan (up-sweep)", "each chunk "
                 "computes its local sum"], BLUE, size=11, lh=16, rx=10))
    b.append(box(430, 90, 300, 60, ["2. final scan (down-sweep)", "add the "
                 "running offset into each chunk"], TEAL, size=11, lh=16,
                 rx=10))
    b.append(arrow(380, 120, 430, 120, GREY))
    b.append(text(W / 2, 200, "prefix[i] = x[0] + x[1] + ... + x[i]", GREY, 12,
                  600, mono=True))
    b.append(text(W / 2, 232, "output: 3  4  8  9 14 23 25 31", TEAL, 13, 600,
                  mono=True))
    b.append(text(W / 2, 300, "scan looks sequential but parallelizes in two "
                  "passes \u2014 body is called with is_final_scan flag", LIGHT,
                  11, 500))
    b.append(text(W / 2, H - 16, "cost: ~2x the work of a serial scan, but "
                  "spread across cores", LIGHT, 11, 500))
    write("figures/parallel-scan.svg", svg(W, H, "".join(b), "parallel_scan"))


@fig
def fig_parallel_pipeline():
    W, H = 820, 340
    b = [text(W / 2, 26, "parallel_pipeline: staged streaming parallelism",
              GREY, 15, 700)]
    stages = [("Input", "serial_in_order", AMBER, "read item"),
              ("Transform", "parallel", TEAL, "heavy work"),
              ("Output", "serial_in_order", AMBER, "write in order")]
    for i, (name, mode, col, note) in enumerate(stages):
        x = 60 + i * 250
        tc = INK_DARK if col == AMBER else WHITE
        b.append(box(x, 90, 200, 60, [name, mode], col, tcol=tc, size=12,
                     lh=17, rx=10))
        b.append(text(x + 100, 172, note, LIGHT, 10, 500))
        if i < 2:
            b.append(arrow(x + 200, 120, x + 60 + 250, 120, GREY))
    b.append(text(150, 230, "token 3", TEAL, 10, 600))
    b.append(text(400, 230, "tokens 1,2 in flight", TEAL, 10, 600))
    b.append(text(660, 230, "token 0 done", TEAL, 10, 600))
    b.append(text(W / 2, 280, "max_tokens caps items in flight (back-pressure); "
                  "parallel stages process many tokens at once", LIGHT, 11,
                  500))
    b.append(text(W / 2, H - 16, "serial_in_order stages preserve input order "
                  "even though the middle stage runs out of order", LIGHT, 11,
                  500))
    write("figures/parallel-pipeline.svg", svg(W, H, "".join(b),
          "parallel_pipeline"))


# -- 02 task programming -----------------------------------------------------
@fig
def fig_task_group():
    W, H = 760, 340
    b = [text(W / 2, 26, "task_group: spawn work, then wait", GREY, 16, 700)]
    b.append(box(300, 60, 160, 44, "task_group tg", BLUE, size=12, rx=10))
    for i in range(3):
        x = 120 + i * 220
        b.append(arrow(380, 104, x + 70, 150, GREY))
        b.append(box(x, 150, 140, 44, [f"tg.run(f{i})"], TEAL, size=11, rx=10,
                     mono=False))
        b.append(box(x, 210, 140, 34, "runs async", TEAL, size=9, rx=6))
        b.append(arrow(x + 70, 194, x + 70, 210, GREY))
    b.append(box(300, 280, 160, 40, "tg.wait()", BLUE_D, size=12, rx=10))
    for i in range(3):
        x = 120 + i * 220
        b.append(arrow(x + 70, 244, 380, 280, GREY, dash="4 4"))
    b.append(text(W / 2, H - 12, "run() may execute immediately or defer; "
                  "wait() blocks until every spawned task in the group finishes",
                  LIGHT, 11, 500))
    write("figures/task-group.svg", svg(W, H, "".join(b), "task_group"))


@fig
def fig_task_arena():
    W, H = 800, 380
    b = [text(W / 2, 26, "task_arena: a bounded slot pool for work", GREY, 16,
              700)]
    b.append(box(60, 70, 320, 250, "", "none", rx=12, stroke=BLUE, sw=1.4))
    b.append(text(220, 90, "arena A  (max_concurrency = 4)", BLUE, 11, 700))
    for i in range(4):
        r, c = divmod(i, 2)
        b.append(box(90 + c * 150, 116 + r * 90, 130, 60, [f"slot {i}",
                     "worker" if i < 3 else "empty"], TEAL if i < 3 else "none",
                     tcol=WHITE if i < 3 else GREY, size=10, lh=14, rx=8,
                     stroke=None if i < 3 else GREY))
    b.append(box(440, 70, 300, 250, "", "none", rx=12, stroke=GREY_D, sw=1.4))
    b.append(text(590, 90, "arena B  (max_concurrency = 2)", GREY_D, 11, 700))
    for i in range(2):
        b.append(box(470, 120 + i * 90, 240, 60, [f"slot {i}", "worker"],
                     BLUE_D, size=10, lh=14, rx=8))
    b.append(text(W / 2, 345, "arenas isolate parallelism: limit a subsystem, "
                  "or keep latency-sensitive work off the main pool", LIGHT, 11,
                  500))
    b.append(text(W / 2, H - 14, "arena.execute([]{ parallel_for(...); }) runs "
                  "the algorithm inside that arena's concurrency limit", LIGHT,
                  11, 500))
    write("figures/task-arena.svg", svg(W, H, "".join(b), "task_arena"))


# -- 03 concurrent containers ------------------------------------------------
@fig
def fig_concurrent_vector():
    W, H = 800, 340
    b = [text(W / 2, 26, "concurrent_vector: segmented, stable addresses", GREY,
              15, 700)]
    segs = [("seg 0", 1, 90), ("seg 1", 2, 170), ("seg 2", 4, 290),
            ("seg 3", 8, 470)]
    for name, n, x in segs:
        b.append(text(x + n * 15, 66, name, LIGHT, 10, 600))
        for i in range(n):
            b.append(box(x + i * 30, 84, 26, 40, "", BLUE, rx=4))
    b.append(text(W / 2, 160, "growth allocates a NEW segment; existing "
                  "elements never move", GREY, 12, 600))
    b.append(box(120, 200, 240, 54, ["push_back() is safe", "concurrently; "
                 "returns an iterator"], TEAL, size=11, lh=16, rx=10))
    b.append(box(440, 200, 240, 54, ["&v[i] stays valid across", "growth "
                 "(unlike std::vector)"], TEAL, size=11, lh=16, rx=10))
    b.append(text(W / 2, 296, "trade-off: elements are NOT contiguous in one "
                  "block \u2192 slightly slower indexing than std::vector",
                  LIGHT, 11, 500))
    b.append(text(W / 2, H - 14, "no concurrent erase; clear/shrink only when no "
                  "other thread touches it", LIGHT, 11, 500))
    write("figures/concurrent-vector.svg", svg(W, H, "".join(b),
          "concurrent_vector"))


@fig
def fig_concurrent_queue():
    W, H = 800, 300
    b = [text(W / 2, 26, "concurrent_queue: many producers & consumers", GREY,
              15, 700)]
    for i in range(3):
        b.append(box(60, 70 + i * 60, 130, 40, f"producer {i}", TEAL, size=10,
                     rx=8))
        b.append(arrow(190, 90 + i * 60, 300, 150, TEAL))
    b.append(box(300, 120, 200, 60, ["concurrent_queue", "(lock-free micro-"
                 "queues)"], BLUE, size=11, lh=16, rx=10))
    for i in range(3):
        b.append(box(610, 70 + i * 60, 130, 40, f"consumer {i}", GREY_D,
                     size=10, rx=8))
        b.append(arrow(500, 150, 610, 90 + i * 60, GREY))
    b.append(text(W / 2, 228, "try_pop() never blocks (returns false if empty); "
                  "use concurrent_bounded_queue for blocking pop + capacity",
                  LIGHT, 11, 500))
    b.append(text(W / 2, H - 16, "no front()/back(): in a concurrent queue they "
                  "would be races by the time you used them", LIGHT, 11, 500))
    write("figures/concurrent-queue.svg", svg(W, H, "".join(b),
          "concurrent_queue"))


@fig
def fig_concurrent_hash_map():
    W, H = 800, 360
    b = [text(W / 2, 26, "concurrent_hash_map: per-bucket accessor locks", GREY,
              15, 700)]
    b.append(box(60, 80, 150, 50, ["key \u2192 hash", "\u2192 bucket"], BLUE,
                 size=11, lh=15, rx=10))
    b.append(arrow(210, 105, 290, 105, GREY))
    for i in range(4):
        y = 70 + i * 60
        b.append(box(300, y, 120, 40, f"bucket {i}", GREY_D, size=10, rx=6))
    b.append(box(480, 90, 250, 60, ["write: accessor", "(exclusive lock on the "
                 "bucket)"], AMBER, tcol=INK_DARK, size=10, lh=15, rx=10))
    b.append(box(480, 180, 250, 60, ["read: const_accessor", "(shared lock "
                 "\u2014 many readers)"], TEAL, size=10, lh=15, rx=10))
    b.append(arrow(420, 110, 480, 120, GREY))
    b.append(arrow(420, 190, 480, 200, GREY))
    b.append(text(W / 2, 290, "the accessor holds the lock; it is released when "
                  "the accessor goes out of scope (RAII)", LIGHT, 11, 500))
    b.append(text(W / 2, H - 16, "pitfall: holding an accessor while locking a "
                  "second key can deadlock \u2014 keep scopes tiny", LIGHT, 11,
                  500))
    write("figures/concurrent-hash-map.svg", svg(W, H, "".join(b),
          "concurrent_hash_map"))


# -- 04 synchronization ------------------------------------------------------
@fig
def fig_mutex_types():
    W, H = 820, 380
    b = [text(W / 2, 26, "Choosing a TBB mutex", GREY, 16, 700)]
    cols = [("Mutex", 40, 190, GREY_D), ("Waiting", 235, 150, BLUE),
            ("Fair?", 390, 110, TEAL), ("Best for", 505, 275, GREY)]
    for name, x, w, col in cols:
        b.append(box(x, 60, w, 32, name, col, size=11, rx=6))
    rows = [
        ("spin_mutex", "busy-spin", "no", "very short critical sections"),
        ("mutex", "OS block", "no", "general purpose, longer sections"),
        ("queuing_mutex", "spin, queued", "yes (FIFO)", "avoiding starvation"),
        ("spin_rw_mutex", "spin", "no", "short read-mostly sections"),
        ("null_mutex", "nothing", "n/a", "template stub / single-threaded"),
    ]
    for i, row in enumerate(rows):
        y = 100 + i * 50
        for (name, x, w, col), cell in zip(cols, row):
            b.append(box(x, y, w, 42, cell, "none", tcol=col, size=10, rx=6,
                         stroke=col, sw=1))
    b.append(text(W / 2, H - 14, "all are scoped: lock via a scoped_lock RAII "
                  "guard so unlock happens on every path", LIGHT, 11, 500))
    write("figures/mutex-types.svg", svg(W, H, "".join(b), "TBB mutex types"))


@fig
def fig_atomic_cas():
    W, H = 780, 340
    b = [text(W / 2, 26, "Lock-free update with compare_exchange", GREY, 15,
              700)]
    b.append(box(300, 64, 200, 40, "old = a.load()", BLUE, size=11, rx=8,
                 mono=False))
    b.append(arrow(400, 104, 400, 132, GREY))
    b.append(box(280, 132, 240, 40, "new = f(old)", TEAL, size=11, rx=8))
    b.append(arrow(400, 172, 400, 200, GREY))
    b.append(box(250, 200, 300, 48, ["compare_exchange(old, new)"], AMBER,
                 tcol=INK_DARK, size=12, rx=10))
    b.append(arrow(550, 224, 650, 224, TEAL))
    b.append(box(650, 204, 110, 40, "success", TEAL, size=11, rx=8))
    b.append(path("M250 224 C160 224 160 88 300 84", GREY, dash="5 4",
                  arrow_end=True))
    b.append(text(150, 160, "retry:", RED, 11, 700))
    b.append(text(180, 178, "someone else", LIGHT, 9, 500))
    b.append(text(180, 192, "changed it", LIGHT, 9, 500))
    b.append(text(W / 2, 288, "the CAS loop is the foundation of every lock-free "
                  "algorithm; contention shows up as more retries", LIGHT, 11,
                  500))
    b.append(text(W / 2, H - 14, "prefer atomic<T> ops over a mutex only for "
                  "tiny scalars; complex state is easier (and often faster) with "
                  "a lock", LIGHT, 11, 500))
    write("figures/atomic-cas.svg", svg(W, H, "".join(b), "Atomic CAS"))


@fig
def fig_rw_lock():
    W, H = 780, 300
    b = [text(W / 2, 26, "Reader-writer lock", GREY, 16, 700)]
    for i in range(3):
        b.append(box(60, 70 + i * 55, 150, 40, f"reader {i}", TEAL, size=10,
                     rx=8))
        b.append(arrow(210, 90 + i * 55, 320, 130, TEAL))
    b.append(box(320, 100, 160, 70, ["shared data"], BLUE, size=12, rx=12))
    b.append(box(600, 110, 130, 50, ["writer", "(exclusive)"], AMBER,
                 tcol=INK_DARK, size=10, lh=14, rx=8))
    b.append(arrow(600, 135, 480, 135, AMBER))
    b.append(text(W / 2, 210, "many readers share the lock; a writer needs it "
                  "exclusively (blocks all readers)", LIGHT, 11, 500))
    b.append(text(W / 2, 238, "win only when reads greatly outnumber writes and "
                  "sections are non-trivial", LIGHT, 11, 500))
    b.append(text(W / 2, H - 14, "upgrade (reader\u2192writer) can fail/retry \u2014 "
                  "handle the false return from upgrade_to_writer()", LIGHT, 11,
                  500))
    write("figures/rw-lock.svg", svg(W, H, "".join(b), "Reader-writer lock"))


# -- 05 memory ---------------------------------------------------------------
@fig
def fig_scalable_allocator():
    W, H = 800, 360
    b = [text(W / 2, 26, "Why a scalable allocator?", GREY, 16, 700)]
    b.append(text(200, 62, "global malloc lock", RED, 12, 700))
    for i in range(4):
        b.append(box(60, 84 + i * 42, 120, 32, f"thread {i}", GREY, size=10,
                     rx=6))
        b.append(arrow(180, 100 + i * 42, 300, 150, RED))
    b.append(box(300, 128, 90, 44, "1 heap", RED, size=11, rx=8))
    b.append(text(200, 268, "serialized \u2192 contention", RED, 10, 600))
    b.append(text(610, 62, "tbbmalloc", TEAL, 12, 700))
    for i in range(4):
        b.append(box(470, 84 + i * 42, 120, 32, f"thread {i}", GREY, size=10,
                     rx=6))
        b.append(arrow(590, 100 + i * 42, 650, 100 + i * 42, TEAL))
        b.append(box(650, 84 + i * 42, 100, 32, "own heap", TEAL, size=9, rx=6))
    b.append(text(610, 268, "per-thread heaps \u2192 no lock on the fast path",
                  TEAL, 10, 600))
    b.append(text(W / 2, H - 14, "use scalable_allocator<T>, tbb::cache_aligned_"
                  "allocator, or link tbbmalloc_proxy to replace malloc", LIGHT,
                  11, 500))
    write("figures/scalable-allocator.svg", svg(W, H, "".join(b),
          "Scalable allocator"))


@fig
def fig_false_sharing():
    W, H = 800, 340
    b = [text(W / 2, 26, "False sharing: two cores, one cache line", GREY, 15,
              700)]
    b.append(box(60, 80, 150, 50, ["core 0", "writes x"], BLUE, size=11,
                 lh=15, rx=10))
    b.append(box(590, 80, 150, 50, ["core 1", "writes y"], TEAL, size=11,
                 lh=15, rx=10))
    b.append(box(300, 90, 200, 44, "cache line (64 B)", AMBER, tcol=INK_DARK,
                 size=11, rx=8))
    b.append(box(310, 100, 40, 24, "x", GREY_D, size=10, rx=4))
    b.append(box(450, 100, 40, 24, "y", GREY_D, size=10, rx=4))
    b.append(path("M210 105 C260 105 260 112 300 112", BLUE, arrow_end=True))
    b.append(path("M590 105 C540 105 540 112 500 112", TEAL, arrow_end=True))
    b.append(text(W / 2, 175, "x and y are independent, but they share ONE line "
                  "\u2192 the line ping-pongs between caches", RED, 11, 600))
    b.append(text(W / 2, 235, "fix: pad/align to 64 B (cache_aligned_allocator, "
                  "alignas(64)) so each hot datum owns its line", LIGHT, 11,
                  500))
    b.append(text(W / 2, H - 16, "symptom: parallel code that scales WORSE than "
                  "serial \u2014 classic false-sharing signature", LIGHT, 11,
                  500))
    write("figures/false-sharing.svg", svg(W, H, "".join(b), "False sharing"))


@fig
def fig_ets():
    W, H = 780, 340
    b = [text(W / 2, 26, "enumerable_thread_specific: local, then combine",
              GREY, 15, 700)]
    for i in range(4):
        x = 60 + i * 180
        b.append(box(x, 70, 150, 40, f"worker {i}", BLUE, size=10, rx=8))
        b.append(box(x, 128, 150, 44, [f"local acc {i}", "(no sharing)"], TEAL,
                     size=10, lh=14, rx=8))
        b.append(arrow(x + 75, 110, x + 75, 128, GREY))
        b.append(arrow(x + 75, 172, W / 2, 240, GREY, dash="4 4"))
    b.append(box(300, 240, 180, 44, "combine(+)", BLUE_D, size=12, rx=10))
    b.append(text(W / 2, H - 30, "each thread mutates its OWN copy lock-free; "
                  "combine()/combine_each() folds them at the end", LIGHT, 11,
                  500))
    b.append(text(W / 2, H - 12, "the pattern behind parallel_reduce \u2014 turns "
                  "a shared-write bottleneck into thread-local writes", LIGHT,
                  11, 500))
    write("figures/ets.svg", svg(W, H, "".join(b), "ETS"))


# -- 06 flow graph -----------------------------------------------------------
@fig
def fig_flow_graph():
    W, H = 800, 320
    b = [text(W / 2, 26, "Flow Graph: computation as a message DAG", GREY, 16,
              700)]
    b.append(box(50, 130, 120, 50, ["input_node", "(source)"], AMBER,
                 tcol=INK_DARK, size=10, lh=14, rx=10))
    b.append(box(230, 70, 140, 50, ["function_node", "square"], TEAL, size=10,
                 lh=14, rx=10))
    b.append(box(230, 190, 140, 50, ["function_node", "cube"], TEAL, size=10,
                 lh=14, rx=10))
    b.append(box(440, 130, 120, 50, ["join_node"], BLUE, size=11, rx=10))
    b.append(box(620, 130, 130, 50, ["function_node", "sum + emit"], BLUE_D,
                 size=10, lh=14, rx=10))
    b.append(arrow(170, 150, 230, 95, GREY))
    b.append(arrow(170, 160, 230, 215, GREY))
    b.append(arrow(370, 95, 440, 145, GREY))
    b.append(arrow(370, 215, 440, 165, GREY))
    b.append(arrow(560, 155, 620, 155, GREY))
    b.append(text(W / 2, 280, "nodes are async actors; edges carry messages. "
                  "the runtime schedules a node when its inputs arrive", LIGHT,
                  11, 500))
    b.append(text(W / 2, H - 14, "use it when dependencies aren't a simple loop "
                  "\u2014 pipelines, fan-out/fan-in, heterogeneous stages",
                  LIGHT, 11, 500))
    write("figures/flow-graph.svg", svg(W, H, "".join(b), "Flow Graph"))


@fig
def fig_join_node():
    W, H = 780, 320
    b = [text(W / 2, 26, "join_node: combine inputs into a tuple", GREY, 16,
              700)]
    b.append(box(60, 90, 150, 40, "port 0: images", TEAL, size=10, rx=8))
    b.append(box(60, 170, 150, 40, "port 1: labels", TEAL, size=10, rx=8))
    b.append(box(320, 120, 160, 70, ["join_node", "<Image, Label>"], BLUE,
                 size=11, lh=16, rx=12))
    b.append(arrow(210, 110, 320, 140, GREY))
    b.append(arrow(210, 190, 320, 170, GREY))
    b.append(arrow(480, 155, 590, 155, BLUE))
    b.append(box(590, 135, 150, 40, "tuple out", BLUE_D, size=11, rx=8))
    b.append(text(W / 2, 235, "policies: queueing (buffer per port) \u00b7 "
                  "reserving (pull when all ready) \u00b7 tag_matching (pair by "
                  "key)", LIGHT, 10, 500))
    b.append(text(W / 2, H - 16, "a join fires only when EVERY input port has a "
                  "message \u2014 natural fan-in synchronization", LIGHT, 11,
                  500))
    write("figures/join-node.svg", svg(W, H, "".join(b), "join_node"))


# -- 07 advanced / performance -----------------------------------------------
@fig
def fig_speedup():
    W, H = 780, 400
    b = [text(W / 2, 26, "Amdahl's law: the serial fraction caps speedup", GREY,
              15, 700)]
    ox, oy, pw, ph = 90, 330, 620, 250
    b.append(line(ox, oy, ox + pw, oy, GREY, 1.4))
    b.append(line(ox, oy, ox, oy - ph, GREY, 1.4))
    b.append(text(ox + pw / 2, oy + 26, "cores \u2192", LIGHT, 11, 600))
    b.append(text(ox - 40, oy - ph / 2, "speedup", LIGHT, 11, 600))
    import math as _m
    def curve(p, col, lab):
        pts = []
        for c in range(1, 17):
            s = 1.0 / ((1 - p) + p / c)
            x = ox + (c / 16.0) * pw
            y = oy - (s / 16.0) * ph
            pts.append((x, y))
        d = "M" + " L".join(f"{x:.0f} {y:.0f}" for x, y in pts)
        b.append(path(d, col, sw=2.0))
        b.append(text(pts[-1][0] - 6, pts[-1][1] - 10, lab, col, 10, 700,
                      anchor="end"))
    curve(1.0, TEAL, "p=100%")
    curve(0.95, BLUE, "p=95%")
    curve(0.75, AMBER, "p=75%")
    curve(0.5, RED, "p=50%")
    b.append(text(W / 2, H - 16, "even 5% serial code limits a 16-core run to "
                  "~9x \u2014 shrink the serial part before adding cores", LIGHT,
                  11, 500))
    write("figures/speedup-amdahl.svg", svg(W, H, "".join(b), "Amdahl"))


@fig
def fig_global_control():
    W, H = 760, 300
    b = [text(W / 2, 26, "global_control: runtime knobs", GREY, 16, 700)]
    b.append(box(80, 90, 280, 60, ["max_allowed_parallelism", "cap total "
                 "worker threads"], BLUE, size=11, lh=16, rx=10))
    b.append(box(400, 90, 280, 60, ["thread_stack_size", "stack for each "
                 "worker"], TEAL, size=11, lh=16, rx=10))
    b.append(text(W / 2, 190, "scoped: the limit holds while the "
                  "global_control object is alive (RAII)", GREY, 12, 600))
    b.append(text(W / 2, 230, "use to leave a core for the OS/UI, or bound "
                  "oversubscription when composing libraries", LIGHT, 11, 500))
    b.append(text(W / 2, H - 16, "prefer task_arena for LOCAL limits; "
                  "global_control is a process-wide ceiling", LIGHT, 11, 500))
    write("figures/global-control.svg", svg(W, H, "".join(b), "global_control"))


def build_font_style(chars):
    if not os.path.exists(FONT_PATH):
        print("WARNING: Virgil.woff2 not found; figures fall back to a system "
              "handwriting font.")
        return ""
    from fontTools.subset import Options, Subsetter
    from fontTools.ttLib import TTFont
    text_ = "".join(sorted(chars))
    opts = Options()
    opts.flavor = "woff2"
    opts.desubroutinize = True
    opts.notdef_outline = True
    opts.recalc_bounds = True
    font = TTFont(FONT_PATH)
    ss = Subsetter(options=opts)
    ss.populate(text=text_)
    ss.subset(font)
    buf = io.BytesIO()
    font.save(buf)
    b64 = base64.b64encode(buf.getvalue()).decode("ascii")
    print(f"embedded font: {len(chars)} glyphs, {len(buf.getvalue())} bytes")
    return ('<style>@font-face{font-family:"Virgil";font-style:normal;'
            'font-weight:400 700;src:url("data:font/woff2;base64,'
            f'{b64}") format("woff2");}}</style>')


if __name__ == "__main__":
    for fn in ALL:
        fn()
    FONT_STYLE = build_font_style(USED_CHARS)
    for fn in ALL:
        fn()
    print(f"\nDone: {len(ALL)} figures generated.")
