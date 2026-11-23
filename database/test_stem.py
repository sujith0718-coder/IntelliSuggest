import re

STEM_RULES = [
    (r'ing$', ''),          # searching -> search, sorting -> sort
    (r'tion$', 'te'),       # authentication -> authenticate
    (r'ed$', ''),           # divided -> divid
    (r's$', ''),            # subproblems -> subproblem, graphs -> graph
]

def stem(word):
    w = word.lower()
    if len(w) <= 3:
        return w
    if w.endswith('ing') and len(w) > 5:
        base = w[:-3]
        if base.endswith('divid') or base.endswith('merg') or base.endswith('balanc') or base.endswith('travers'):
            return base + 'e'
        return base
    if w.endswith('tion') and len(w) > 6:
        return w[:-4] + 'te'
    if w.endswith('ed') and len(w) > 4:
        base = w[:-2]
        if base.endswith('divid') or base.endswith('balanc'):
            return base + 'e'
        return base
    if w.endswith('es') and len(w) > 4:
        if w == 'halves':
            return 'half'
        return w[:-2]
    if w.endswith('s') and not w.endswith('ss') and len(w) > 3:
        return w[:-1]
    return w

test_words = [
    'dividing', 'divides', 'divided', 'merging', 'merges', 'sorting', 'sorts',
    'subproblems', 'graphs', 'cycles', 'authentication', 'searching'
]
print({w: stem(w) for w in test_words})

