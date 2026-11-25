#!/usr/bin/env python3
"""xbased's shipped cheat and patch database, built from community archives.

  build_db.py <etm dir | -> <patch repo dir>... --xdb <xdb info.json dir> --out <repo>

- Cheats: the Xbox scene's .etm/.xbtf files (EvolutionX / XBMC4Xbox
  "trainers"), each read as xbased reads it (native/core/xcheat.c) and copied
  to <repo>/cheats/<TITLE ID>/ for each title it names (identical files once).
- Patches: IPS patches named after their game ("Conker - Live & Reloaded (60
  FPS Patch).ips"), matched to a title ID by name through xemu's xdb, copied
  to <repo>/mods/<TITLE ID>/ as "<kind>.ips" (720p patches left out: their
  authors call them unstable). Only to the USA releases' title IDs (the
  Original edition's where there are several): the patches were made and
  tested on the USA versions, and an IPS patch writes bytes at fixed offsets
  of default.xbe, so on another release's executable it lands on other code.
  Grand Theft Auto: San Andreas's 60 FPS patch, given to its European Classics
  release too, overwrote half of a call there and the game crashed while
  loading. A release whose region xdb doesn't give keeps its patches (one
  title ID for all). OgXHD's widescreen database goes to
  <repo>/mods/widescreen.dat as it is.
- A patch taken away from a release is listed in <repo>/mods/removed.txt
  (its path, size and FNV-1a 64 hash): Windows' update unpacks the new
  package over the folder and deletes nothing, so xbased deletes those files
  itself when they're still the database's (native/app/win_mods.c).
- Cheats that don't check the build they were made for (the XBE header's
  build time, the dword at 0x10114, which nearly all compare before they
  patch) are left out: they write to fixed places whatever the build, and a
  release's title ID can be shared by builds made apart. They're listed in
  <repo>/cheats/removed.txt for the same clean-up.
- The patches placed are listed in <repo>/mods/patches.txt (path, size,
  FNV-1a 64): xbased offers those only on an executable sold in North
  America whose code they fit (native/app/mod_fit.c); the user's own as
  they are.
- An etm dir of "-" leaves the cheats as they are.
"""
import argparse, hashlib, json, os, re, shutil, struct, sys, unicodedata


def u32(b, o):
    return struct.unpack_from('<I', b, o)[0]


def unscramble(b):
    b = bytearray(b)
    s = (b[0x27] + b[0x2F] + b[0x37]) & 0xFF
    struct.pack_into('<I', b, 0, u32(b, 0) ^ ((s * 0xFFFFFF) & 0xFFFFFFFF))
    key, sub, left = b[0], 0, len(b) - 4
    for i in range(4, len(b)):
        b[i] ^= key
        b[i] = (b[i] - (sub & 0xFF)) & 0xFF
        sub += 3 + left
        left -= 1
    return bytes(b)


def read_cheat(path):
    """(name, title ids, options) or None, as nc_xcheat_load reads it"""
    raw = open(path, 'rb').read()
    if len(raw) < 0x40 or len(raw) > 1 << 20:
        return None
    data = raw
    if path.lower().endswith('.xbtf'):
        raw = unscramble(raw)
        k = raw.find(b'\0', 4)
        if k < 0 or k - 4 < 7 or raw[10] != ord('-'):
            return None
        t = raw.find(b'\0', k + 1)
        data = raw[t + 1:]
        if len(data) < 0x1E:
            return None
    opts, texts, ids = u32(data, 0x0A), u32(data, 0x0E), u32(data, 0x12)
    if texts > len(data) or opts >= texts or ids + 12 > len(data):
        return None
    tids = [u32(data, ids + 4 * i) for i in range(3)]
    strs = []
    for i in range(texts - opts + 2):
        if texts + 4 * i + 4 > len(data):
            break
        off = u32(data, texts + 4 * i)
        if not off:
            break
        if off >= len(data):
            return None
        e = data.find(b'\0', off)
        strs.append(data[off:e if e >= 0 else len(data)].decode('latin1', 'replace').strip())
    if len(strs) < 3:
        return None
    # a title ID begins with its publisher's two letters (MS: 4D53)
    real = [t for t in tids if 0x41 <= t >> 24 <= 0x5A and 0x41 <= (t >> 16) & 0xFF <= 0x5A]
    return strs[0], real, strs[2:]


def cheat_body(path):
    """the cheat itself, as nc_xcheat_load reads it (an .xbtf's past its key and text)"""
    raw = open(path, 'rb').read()
    if path.lower().endswith('.xbtf'):
        raw = unscramble(raw)
        k = raw.find(b'\0', 4)
        t = raw.find(b'\0', k + 1)
        raw = raw[t + 1:]
    return raw


XOR_HEAD = re.compile(rb'\xBF\x14\x01\x01\x00\x8D\xB5(....)\x8D\x8D(....)\x8D\x95(....)'
                      rb'\x2B\xCE\x8B\x1D\x44\x01\x01\x00\xC1\xE9\x02', re.S)


def checks_build(path):
    """does the cheat compare the build it was made for (the XBE header's
    build time, at 0x10114) before it patches: as native/app/mod_fit.c's
    fit_cheat_builds decides it (a cmp of [0x10114], or of a register loaded
    from it within three instructions; or the XOR-obscured form)"""
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32   # pip install capstone
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    b = cheat_body(path)
    if XOR_HEAD.search(b):
        return True
    regs = ('eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp')
    k = -1
    while True:
        k = b.find(b'\x14\x01\x01\x00', k + 1)
        if k < 0:
            return False
        for back in range(1, min(8, k + 1)):
            ins = list(md.disasm(b[k - back:k + 48], k - back))
            if not ins or ins[0].address + ins[0].size <= k or '0x10114' not in ins[0].op_str:
                continue
            i0 = ins[0]
            if i0.mnemonic == 'cmp' and i0.op_str.startswith('dword ptr [0x10114],'):
                if re.search(r', -?0x[0-9a-f]+$|, \d+$', i0.op_str):
                    return True
                break
            reg = i0.op_str.split(',')[0].strip()
            if i0.mnemonic != 'mov' or reg not in regs:
                continue
            for nx in ins[1:4]:
                if nx.mnemonic == 'cmp' and (nx.op_str.startswith(reg + ',') or
                                             nx.op_str.startswith('dword ptr [%s],' % reg)):
                    if re.search(r', -?0x[0-9a-f]+$|, \d+$', nx.op_str):
                        return True
                    break
            break


def fnv1a64(b):
    h = 0xCBF29CE484222325
    for c in b:
        h = ((h ^ c) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def norm(s):
    s = unicodedata.normalize('NFKD', s).encode('ascii', 'ignore').decode().lower()
    s = s.replace('&', ' and ')
    s = re.sub(r"[’'`]", '', s)
    s = re.sub(r'\b(the|a|an)\b', ' ', s)
    s = re.sub(r'[^a-z0-9]+', ' ', s)
    return ' '.join(s.split())


KINDS = [  # the patch's kind from its name, the file name it gets
    (r'16[-:x]9|widescreen', 'Widescreen 16-9'),
    (r'60 ?fps', '60 FPS'),
    (r'480p|progressive', '480p'),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('etm')
    ap.add_argument('patches', nargs='+')
    ap.add_argument('--xdb', required=True)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()

    # ---- cheats
    cheats = os.path.join(a.out, 'cheats')
    if a.etm != '-':
        shutil.rmtree(cheats, ignore_errors=True)
    seen, ok, bad, titles, unchecked = set(), 0, 0, set(), set()
    for root, _, files in (os.walk(a.etm) if a.etm != '-' else []):
        for f in sorted(files):
            if not f.lower().endswith(('.etm', '.xbtf')):
                continue
            p = os.path.join(root, f)
            c = read_cheat(p)
            if not c or not c[1]:
                bad += 1
                continue
            raw = open(p, 'rb').read()
            h = hashlib.sha1(raw).hexdigest()
            if h in seen:
                continue
            seen.add(h)
            if not checks_build(p):
                for t in c[1]:
                    name = re.sub(r'[<>:"/\\|?*]', ' ', f).strip()
                    unchecked.add('%08X/%s %d %016x' % (t, name, len(raw), fnv1a64(raw)))
                continue
            ok += 1
            for t in c[1]:
                d = os.path.join(cheats, '%08X' % t)
                os.makedirs(d, exist_ok=True)
                name = re.sub(r'[<>:"/\\|?*]', ' ', f).strip()
                shutil.copyfile(p, os.path.join(d, name))
                titles.add(t)
    if a.etm != '-':
        os.makedirs(cheats, exist_ok=True)
        with open(os.path.join(cheats, 'removed.txt'), 'w', encoding='utf-8', newline='\n') as o:
            o.write('# Cheats left out of the database (they patch without checking the build\n'
                    "# they were made for): <title id>/<file> <size> <FNV-1a 64>, deleted at\n"
                    '# start while unchanged (native/app/win_mods.c)\n')
            for g in sorted(unchecked):
                o.write(g + '\n')
    print('cheats: %d files for %d titles (%d unreadable or for no title, %d left out: no build check)' %
          (ok, len(titles), bad, len(unchecked)))

    # ---- names to title ids (xdb): every release's (an old placement, removed
    # below) and the USA ones' (the patches')
    def tid(i):
        return i.upper() if re.fullmatch(r'[0-9A-Fa-f]{8}', i or '') else None

    names, usa_names = {}, {}
    for f in os.listdir(a.xdb):
        try:
            j = json.load(open(os.path.join(a.xdb, f), encoding='utf-8'))
        except Exception:
            continue
        rels = [r for r in j.get('releases', []) if tid(r.get('title_id'))]
        ids = {tid(j.get('title_id'))} | {tid(r['title_id']) for r in rels}
        ids.discard(None)
        usa = [r for r in rels if re.search(r'USA|World', r.get('region', '')) and
               'demo' not in r.get('edition', '').lower()]
        orig = [r for r in usa if 'original' in r.get('edition', '').lower()]
        if usa:
            usa_ids = {tid(r['title_id']) for r in (orig or usa)}
        elif any(r.get('region') for r in rels):
            usa_ids = set()                     # no USA release: untested
        else:
            usa_ids = set(ids)                  # regions unknown
        for n in [j.get('name', '')] + [r.get('name', '') for r in j.get('releases', []) if r.get('name')]:
            if n and ids:
                names.setdefault(norm(n), set()).update(ids)
                usa_names.setdefault(norm(n), set()).update(usa_ids)

    # ---- patches
    placed, removed, unmatched = 0, 0, []
    rm_list = os.path.join(a.out, 'mods', 'removed.txt')
    gone = set()
    if os.path.exists(rm_list):
        gone = {l.strip() for l in open(rm_list, encoding='utf-8') if l.strip() and not l.startswith('#')}
    done, shipped = set(), set()
    for repo in a.patches:
        for root, _, files in os.walk(repo):
            if '720p' in root:
                continue
            for f in sorted(files):
                if not f.lower().endswith('.ips'):
                    continue
                m = re.match(r'(.*?)\s*\((.*)\)\.ips$', f, re.I)
                game, rest = (m.group(1), m.group(2)) if m else (f[:-4], '')
                if '720p' in rest:
                    continue
                kind = next((k for r, k in KINDS if re.search(r, rest, re.I)), None)
                if not kind:
                    kind = re.sub(r'[^A-Za-z0-9 .+-]', '', rest.split(')')[0]).strip() or 'Patch'
                extra = re.findall(r'\(([^)]*)\)', '(' + rest + ')')[1:]
                if extra:
                    kind += ' (' + ', '.join(extra) + ')'
                kind = re.sub(r'[<>:"/\\|?*]', ' ', kind).strip()
                ids = names.get(norm(game))
                if not ids:
                    unmatched.append(f)
                    continue
                src = os.path.join(root, f)
                body = open(src, 'rb').read()
                usa_ids = usa_names.get(norm(game), set())
                for t in sorted(ids - usa_ids):
                    # an earlier placement on another release: gone (only an
                    # identical file, never a curated mod of the same name)
                    old = os.path.join(a.out, 'mods', t, kind + '.ips')
                    if os.path.exists(old) and open(old, 'rb').read() == body:
                        os.remove(old)
                        removed += 1
                        gone.add('%s/%s.ips %d %016x' % (t, kind, len(body), fnv1a64(body)))
                        if not os.listdir(os.path.dirname(old)):
                            os.rmdir(os.path.dirname(old))
                for t in sorted(usa_ids):
                    # a release's own tested 60 fps mod (mods/<id>.<v>/60fps) over a generic patch
                    mods = os.path.join(a.out, 'mods')
                    if kind.startswith('60 FPS') and any(
                            x.upper().startswith(t + '.') and os.path.isdir(os.path.join(mods, x, '60fps'))
                            for x in os.listdir(mods)):
                        # an earlier run's copy stays one of the database's
                        old = os.path.join(mods, t, kind + '.ips')
                        if os.path.exists(old) and open(old, 'rb').read() == body:
                            shipped.add('%s/%s.ips %d %016x' % (t, kind, len(body), fnv1a64(body)))
                        continue
                    d = os.path.join(a.out, 'mods', t)
                    dst = os.path.join(d, kind + '.ips')
                    if dst.lower() in done:
                        continue
                    done.add(dst.lower())
                    os.makedirs(d, exist_ok=True)
                    shutil.copyfile(src, dst)
                    placed += 1
                    shipped.add('%s/%s.ips %d %016x' % (t, kind, len(body), fnv1a64(body)))
    with open(rm_list, 'w', encoding='utf-8', newline='\n') as o:
        o.write('# Patches the database took away from these releases (made for another\n'
                "# release's executable): <title id>/<file> <size> <FNV-1a 64>, deleted at\n"
                '# start while unchanged (native/app/win_mods.c)\n')
        for g in sorted(gone):
            o.write(g + '\n')
    with open(os.path.join(a.out, 'mods', 'patches.txt'), 'w', encoding='utf-8', newline='\n') as o:
        o.write("# The database's patches, made for the North American releases:\n"
                '# <title id>/<file> <size> <FNV-1a 64>. Offered only on an executable sold\n'
                '# there whose code they fit (native/app/mod_fit.c)\n')
        for g in sorted(shipped):
            o.write(g + '\n')
    print('patches: %d placed, %d removed from non-USA releases, %d not matched to a title' %
          (placed, removed, len(unmatched)))
    for u in unmatched:
        print('  unmatched:', u)

    for repo in a.patches:
        w = os.path.join(repo, '!OgXHD', 'wdb.dat')
        if os.path.exists(w):
            shutil.copyfile(w, os.path.join(a.out, 'mods', 'widescreen.dat'))
            print('widescreen.dat from', w)
            break


if __name__ == '__main__':
    main()
