#!/usr/bin/env python3
"""Rewrite the call sites of selected lifted functions so their frames are recorded (and later resumable).

Usage: make_resumable.py FUNCTION_LIST OUTPUT_DIR SOURCE...
FUNCTION_LIST holds one hex guest address per line. Sources without a selected function are
passed through untouched; rewritten copies go to OUTPUT_DIR. Prints the source list to compile,
one path per line, and a summary on stderr. Fails if a selected function has a call form it
does not understand, so a frame can never be silently missing."""
import pathlib
import re
import sys

CALL = re.compile(r'^(\s*)RECOMP_CALL\((sub_([0-9A-F]{8}))\);(\s*/\* 0x([0-9A-F]{8}): .*)$')
ICALL = re.compile(r'^(\s*)RECOMP_ICALL\((.*)\);(\s*/\* 0x([0-9A-F]{8}): .*)$')
ITAIL = re.compile(r'^(\s*)RECOMP_ITAIL\((.*)\); RECOMP_RETURN\(\);(\s*/\* 0x([0-9A-F]{8}): .*)$')
SWITCH_TAIL = re.compile(r'^(\s*)default: RECOMP_ITAIL\((_jt)\); RECOMP_RETURN\(\);$')
JUMP_SITE = re.compile(r'/\* 0x([0-9A-F]{8}): jmp ')
HEAD = re.compile(r'^void sub_([0-9A-F]{8})\(void\) \{$')
CALL_FORMS = ('RECOMP_CALL', 'RECOMP_ICALL', 'RECOMP_ITAIL')


def rewrite(lines, selected, stats):
    out, current, last_jump = [], None, None
    for number, line in enumerate(lines, 1):
        head = HEAD.match(line)
        if head:
            current = int(head[1], 16) if int(head[1], 16) in selected else None
            if current is not None:
                stats['functions'].add(current)
        elif line.startswith('}'):
            current = None
        if jump := JUMP_SITE.search(line):
            last_jump = jump[1]
        if current is None or not any(form in line for form in CALL_FORMS):
            out.append(line)
            continue
        if m := CALL.match(line):
            out.append(f'{m[1]}NFS_RCALL(0x{m[5]}u, 0x{m[3]}u, {m[2]});{m[4]}')
        elif m := ITAIL.match(line):
            out.append(f'{m[1]}NFS_RITAIL(0x{m[4]}u, {m[2]}); RECOMP_RETURN();{m[3]}')
        elif (m := SWITCH_TAIL.match(line)) and last_jump:
            # A switch-table jump whose target is not a case of this function: the table jump is the site.
            out.append(f'{m[1]}default: NFS_RITAIL(0x{last_jump}u, {m[2]}); RECOMP_RETURN();')
        elif m := ICALL.match(line):
            out.append(f'{m[1]}NFS_RICALL(0x{m[4]}u, {m[2]});{m[3]}')
        else:
            raise SystemExit(f'Unrecognized call form in sub_{current:08X} (line {number}): {line.strip()}')
        stats['sites'] += 1
    return out


def main():
    if len(sys.argv) < 4:
        raise SystemExit(__doc__)
    selected = {int(x, 16) for x in open(sys.argv[1]) if x.strip()}
    output = pathlib.Path(sys.argv[2])
    output.mkdir(parents=True, exist_ok=True)
    stats = {'functions': set(), 'sites': 0}
    for source in map(pathlib.Path, sys.argv[3:]):
        text = source.read_text()
        heads = {int(a, 16) for a in re.findall(r'^void sub_([0-9A-F]{8})\(void\) \{$', text, re.M)}
        if not heads & selected:
            print(source)
            continue
        target = output / source.name
        rewritten = '\n'.join(rewrite(text.split('\n'), selected, stats))
        if not target.exists() or target.read_text() != rewritten:
            target.write_text(rewritten)
        print(target)
    missing = selected - stats['functions']
    print(f"resumable: {len(stats['functions'])} functions, {stats['sites']} call sites; "
          f"{len(missing)} listed addresses have no lifted body", file=sys.stderr)


if __name__ == '__main__':
    main()
