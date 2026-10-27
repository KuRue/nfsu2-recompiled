#!/usr/bin/env python3
"""Rewrite the call sites of selected lifted functions so their frames are recorded and can be resumed.

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


ENTER = re.compile(r'^(\s*)RECOMP_ENTER\(0x([0-9A-F]{8})u\);')


def rewrite_function(body, address, number):
    """Label every call site and add a resume dispatch after RECOMP_ENTER."""
    out, sites, last_jump = [], [], None
    for offset, line in enumerate(body):
        if jump := JUMP_SITE.search(line):
            last_jump = jump[1]
        if not any(form in line for form in CALL_FORMS):
            out.append(line)
            continue
        if m := CALL.match(line):
            site, text = m[5], f'{m[1]}R_{m[5]}: NFS_RCALL(0x{m[5]}u, 0x{m[3]}u, {m[2]});{m[4]}'
        elif m := ITAIL.match(line):
            site, text = m[4], f'{m[1]}R_{m[4]}: NFS_RITAIL(0x{m[4]}u, {m[2]}); RECOMP_RETURN();{m[3]}'
        elif (m := SWITCH_TAIL.match(line)) and last_jump:
            # A switch-table jump whose target is not a case of this function: the table jump is the site.
            site, text = last_jump, f'{m[1]}default: R_{last_jump}: NFS_RITAIL(0x{last_jump}u, {m[2]}); RECOMP_RETURN();'
        elif m := ICALL.match(line):
            site, text = m[4], f'{m[1]}R_{m[4]}: NFS_RICALL(0x{m[4]}u, {m[2]});{m[3]}'
        else:
            raise SystemExit(f'Unrecognized call form in sub_{address:08X} (line {number + offset}): {line.strip()}')
        if site in sites:
            raise SystemExit(f'Call site 0x{site} appears twice in sub_{address:08X}')
        sites.append(site)
        out.append(text)
    for index, line in enumerate(out):
        if m := ENTER.match(line):
            if int(m[2], 16) != address:
                raise SystemExit(f'sub_{address:08X} enters as 0x{m[2]}')
            cases = ' '.join(f'case 0x{site}u: goto R_{site};' for site in sites)
            out.insert(index + 1, f'{m[1]}NFS_RESUME_ENTRY(0x{address:08X}u) {{ switch (nfs_resume_site) {{ {cases} default: nfs_resume_fail(); }} }}')
            return out, len(sites)
    raise SystemExit(f'sub_{address:08X} has no RECOMP_ENTER')


def rewrite(lines, selected, stats):
    out, current, body, start = [], None, [], 0
    for number, line in enumerate(lines, 1):
        head = HEAD.match(line)
        if head and int(head[1], 16) in selected:
            current, body, start = int(head[1], 16), [], number + 1
            stats['functions'].add(current)
            out.append(line)
            continue
        if current is not None:
            if line.startswith('}'):
                rewritten, count = rewrite_function(body, current, start)
                out += rewritten
                stats['sites'] += count
                current = None
            else:
                body.append(line)
                continue
        out.append(line)
    if current is not None:
        raise SystemExit(f'sub_{current:08X} is not closed')
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
