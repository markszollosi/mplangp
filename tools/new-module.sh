#!/bin/sh
# usage: tools/new-module.sh name   (edits the repo the script lives in)
# Creates modules/name.c, docs page + index rows, sample + expected, error check.
root=$(cd "$(dirname "$0")/.." && pwd)
n=$1
case "$n" in
'' | *[!a-z0-9_]* | [0-9]*) echo "usage: $0 name (lowercase letters, digits, _)" >&2; exit 2 ;;
esac
cd "$root" || exit 1
[ -e "modules/$n.c" ] && { echo "modules/$n.c exists" >&2; exit 1; }
cat > "modules/$n.c" <<EOT
#include "mplangpfunc.h"

static bool m_ping(int argc, Value *argv, Value *ret)
{
	(void)argv;
	if (argc != 0)
		return mp_error(ret, "ping() takes no arguments.");
	*ret = mp_str("pong");
	return true;
}

MP_MODULE({"ping", m_ping});
EOT
cat > "docs/modules/$n.md" <<EOT
# $n: TODO one-line purpose

\`import $n\`. TODO describe.

| function | returns | notes |
|---|---|---|
| \`ping()\` | str | placeholder, replace with the real functions |

\`\`\`my
import $n

print($n.ping())
\`\`\`

\`\`\`output
pong
\`\`\`
EOT
printf 'import %s\n\nprint(%s.ping())\n' "$n" "$n" > "tests/samples/$n.my"
printf 'pong\n' > "tests/samples/$n.expected"
awk -v n="$n" '
	/^\| \[[a-z0-9_]+\]\([a-z0-9_]+\.md\) \| `/ { last = NR }
	{ line[NR] = $0 }
	END { for (i = 1; i <= NR; i++) { print line[i]; if (i == last) printf "| [%s](%s.md) | `ping` |\n", n, n } }
' docs/modules/README.md > docs/modules/README.md.new && mv docs/modules/README.md.new docs/modules/README.md
c="check 70 \"ping() takes no arguments.\" 'import $n\\n$n.ping(1)\\n'" awk '/^exit \$fail$/ { print ENVIRON["c"] } { print }' tests/errors.sh > tests/errors.sh.new
cat tests/errors.sh.new > tests/errors.sh && rm tests/errors.sh.new
echo "created modules/$n.c docs/modules/$n.md tests/samples/$n.{my,expected}; edit them (add LIB_$n to Makefile if it needs a library), then make test"
