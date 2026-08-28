"""Convert the Nexus page description to the plain text readme shipped in the
release archive, so both come from one source and cannot drift apart.

Usage: nexus_to_text.py <input.bbcode> [output.txt] [--title "Name 1.2.3"]
"""
import re
import sys

WIDTH = 78


def expand_url(match):
    target, label = match.group(1), match.group(2)
    # A label that is already the address needs no second copy of it.
    if target.split("://", 1)[-1].rstrip("/") == label.rstrip("/"):
        return target
    return f"{label} <{target}>"


def strip_tags(text):
    # A text file cannot link, so the address is spelled out.
    text = re.sub(r"\[url=([^\]]+)\]([^\[]+)\[/url\]", expand_url, text)
    text = re.sub(r"\[/?(?:b|i|u|size(?:=\d+)?|color(?:=[^\]]+)?)\]", "", text)
    return text


def wrap(paragraph, indent=""):
    words = paragraph.split()
    lines, current = [], indent
    for word in words:
        candidate = word if current == indent else f"{current} {word}"
        if len(candidate) > WIDTH and current != indent:
            lines.append(current)
            current = indent + word
        else:
            current = candidate
    if current != indent:
        lines.append(current)
    return lines


HEADING = re.compile(r"^\[size=\d+\]\[b\](.+?)\[/b\]\[/size\]$")


def convert(source, title=None):
    out = []
    in_code = False

    if title:
        out += [title, "=" * len(title), ""]
    tagline_seen = False

    for raw in source.splitlines():
        line = raw.rstrip()

        if "[code]" in line:
            in_code = True
            line = line.replace("[code]", "")
        if "[/code]" in line:
            line = line.replace("[/code]", "")
            in_code = False
            if line.strip():
                out.append("    " + strip_tags(line).strip())
            continue
        if in_code:
            out.append("    " + strip_tags(line).strip())
            continue

        if line.strip() in ("[list]", "[/list]"):
            continue

        heading = HEADING.match(line.strip())
        if heading:
            text = strip_tags(heading.group(1)).strip()
            # The first one is the tagline, not a section title.
            if not tagline_seen:
                tagline_seen = True
                out += wrap(text)
            else:
                out += [text, "-" * len(text)]
            continue

        line = strip_tags(line)

        if not line.strip():
            out.append("")
            continue

        if line.strip().startswith("[*]"):
            out.extend(wrap("- " + line.strip()[3:], indent="  ")[:1]
                       + wrap(line.strip()[3:], indent="  ")[1:])
            continue

        out.extend(wrap(line.strip()))

    # collapse runs of blank lines left by removed markup
    collapsed = []
    for line in out:
        if line == "" and collapsed and collapsed[-1] == "":
            continue
        collapsed.append(line)
    return "\n".join(collapsed).strip() + "\n"


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    title = None
    if "--title" in sys.argv:
        title = sys.argv[sys.argv.index("--title") + 1]
    text = convert(open(sys.argv[1], encoding="utf-8").read(), title)
    if len(sys.argv) > 2 and not sys.argv[2].startswith("--"):
        # CRLF: the file ships to Windows users and may be opened by anything.
        with open(sys.argv[2], "w", encoding="utf-8", newline="\r\n") as f:
            f.write(text)
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
