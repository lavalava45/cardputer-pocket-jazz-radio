"""Generate a local, dependency-free 240x135 Help layout preview from firmware.

This is a layout mock, NOT an ESP32/Velxio emulator. It never flashes hardware,
starts a network request, or modifies existing release archives.
"""
from __future__ import annotations

import html
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "PocketJazzRadio" / "PocketJazzRadioApp.cpp"
OUTPUT = ROOT / "preview" / "help-v07k.html"


def extract_pages(source: str) -> list[dict[str, object]]:
    match = re.search(
        r"static constexpr HelpPage pages\[\] = \{(.*?)\};\s*static_assert",
        source,
        re.S,
    )
    if not match:
        raise ValueError("HelpPage table missing or relocated")
    page_count = re.search(r"constexpr uint8_t HELP_PAGE_COUNT\s*=\s*(\d+);", source)
    if not page_count:
        raise ValueError("Help page count missing")
    declared_count = int(page_count.group(1))
    result = []
    for category, raw in re.findall(
        r'\{"([^"]+)",\s*\{(.*?)\}\}', match.group(1), re.S
    ):
        rows = re.findall(r'"([^"]+)"', raw)
        if len(rows) != 4 or any(len(s) > 18 for s in rows):
            raise ValueError(f"Invalid four-row Help layout in {category!r}")
        heading = f"HELP {len(result)+1}/{declared_count}  {category}"
        if 8 + len(heading) * 6 > 232:
            raise ValueError(f"Heading overflows native 240px LCD: {heading}")
        result.append({"section": category, "rows": rows})
    if len(result) != declared_count:
        raise ValueError("Help page count disagrees with firmware")
    return result


def render(output: Path | None = None) -> Path:
    pages = extract_pages(APP.read_text(encoding="utf-8"))
    dataset = json.dumps(pages, ensure_ascii=True, separators=(",", ":"))
    source_note = html.escape(APP.name)
    content = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Pocket Jazz Radio — offline Help layout preview</title>
<style>
html,body{background:#141820;color:#fff;font:14px system-ui,sans-serif;margin:0}
main{max-width:720px;margin:28px auto;padding:0 16px}
canvas{width:min(100%,720px);height:auto;image-rendering:pixelated;border:2px solid #667085;background:#071624}
.controls{display:flex;align-items:center;gap:14px;flex-wrap:wrap;margin:18px 0}
button{background:#293850;color:white;border:1px solid #7e97af;border-radius:6px;padding:9px 14px;cursor:pointer}
button:focus-visible{outline:2px solid white}
.note{color:#aebccc;font-size:13px;max-width:640px;line-height:1.5}
</style>
</head>
<body>
<main>
<h1>Pocket Jazz Radio — Help preview</h1>
<canvas id="lcd" width="240" height="135" role="img" aria-label="240 by 135 LCD Help mockup"></canvas>
<div class="controls">
<button id="prev" type="button">↑ Previous</button>
<span id="counter"></span>
<button id="next" type="button">↓ Next</button>
<button id="back" type="button">Del / Back</button>
</div>
<p class="note">Offline layout-only preview. ArrowUp/ArrowDown or the buttons switch
help pages, and Delete/Backspace returns to a RADIO placeholder. This mock uses
browser fonts; physical M5GFX pixel metrics, keyboard mapping, SPI timing,
Wi-Fi, audio, SD and NVS are NOT simulated. The 17 pages are extracted
automatically from <code>__SOURCE__</code>, not retyped.</p>
</main>
<script>
"use strict";
const pages=__PAGES__;
const cvs=document.getElementById("lcd"),ctx=cvs.getContext("2d");
ctx.textBaseline="top";
let page=0,help=true;
function line(y){ctx.fillStyle="#63798b";ctx.fillRect(8,y,224,1)}
function text(x,y,s,big=false){
  ctx.fillStyle="#ffffff";
  // M5GFX builtin font cell is 6x8px; textSize=2 occupies 12x16px/cell.
  // Browser monospace glyph shapes differ, but fixed cell spacing matches
  // the firmware's 240x135 geometry and exposes horizontal overflow.
  ctx.font=(big?"16":"8")+"px monospace";
  const stride=big?12:6;
  for(let i=0;i<s.length;i++)ctx.fillText(s[i],x+i*stride,y);
}
function draw(){
  ctx.fillStyle="#071624";ctx.fillRect(0,0,240,135);
  if(help){
    const p=pages[page];
    text(8,3,"HELP "+(page+1)+"/"+pages.length+"  "+p.section);
    line(24);
    p.rows.forEach((r,i)=>text(8,29+i*20,r,true));
    line(110);
    text(8,121,"UP/DOWN PAGES   DEL BACK");
    document.getElementById("counter").textContent=(page+1)+" / "+pages.length;
  }else{
    text(8,10,"RADIO — LAYOUT PREVIEW ONLY",true);
    text(8,50,"Press H to reopen Help");
    document.getElementById("counter").textContent="Radio";
  }
}
function go(delta){help=true;page=(page+delta+pages.length)%pages.length;draw()}
document.getElementById("prev").addEventListener("click",()=>go(-1));
document.getElementById("next").addEventListener("click",()=>go(1));
document.getElementById("back").addEventListener("click",()=>{help=false;draw()});
document.addEventListener("keydown",(e)=>{
  if(["ArrowUp","ArrowDown","Delete","Backspace","h","H"].includes(e.key))e.preventDefault();
  if(e.key==="ArrowUp")go(-1);
  else if(e.key==="ArrowDown")go(1);
  else if(e.key==="Delete"||e.key==="Backspace"){help=false;draw()}
  else if(e.key==="h"||e.key==="H"){help=true;draw()}
});
draw();
</script>
</body>
</html>
"""
    # JSON backslash sequences are safe in a literal JS expression. Our Help
    # strings have neither </script> nor backticks; fail closed if that changes.
    if any("</" in value for page in pages for value in
           [page["section"], *page["rows"]]):
        raise ValueError("Unexpected HTML/script delimiter in Help pages")
    destination = output if output is not None else OUTPUT
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(
        content.replace("__SOURCE__", source_note)
               .replace("__PAGES__", dataset)
               .replace("The 17 pages", f"The {len(pages)} pages"),
        encoding="utf-8",
    )
    return destination


if __name__ == "__main__":
    print(render())
