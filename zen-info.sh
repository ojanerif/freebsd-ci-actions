# zen-info.sh — AMD Zen identification + IBS feature dump for FreeBSD.
#
# Reads CPUID via cpucontrol(8) and reports:
#   - Family / Model / Stepping
#   - Best-effort Zen generation and public codename
#   - Full IBS feature decode (CPUID Fn8000_001B EAX)
#   - hwpmc IBS sysctl state, if hwpmc.ko is loaded
#
# Run as root:   sudo sh zen-info.sh

set -u

CPU=${CPU:-0}
DEV="/dev/cpuctl${CPU}"

# ---- prerequisites -----------------------------------------------------------

if [ "$(uname -s)" != "FreeBSD" ]; then
	echo "WARNING: not FreeBSD ($(uname -s)); cpucontrol won't work." >&2
fi

if [ ! -e "$DEV" ]; then
	echo "Loading cpuctl module..." >&2
	kldload cpuctl 2>/dev/null || true
fi
if [ ! -e "$DEV" ]; then
	echo "ERROR: $DEV not present (need root + cpuctl(4))." >&2
	exit 1
fi

if ! command -v cpucontrol >/dev/null 2>&1; then
	echo "ERROR: cpucontrol(8) not found." >&2
	exit 1
fi

# ---- helpers -----------------------------------------------------------------

# read_cpuid LEAF -> sets EAX EBX ECX EDX (decimal)
read_cpuid() {
	_leaf=$1
	set -- $(cpucontrol -i "$_leaf" "$DEV" | awk '{print $4, $5, $6, $7}')
	EAX=$(printf '%d' "$1")
	EBX=$(printf '%d' "$2")
	ECX=$(printf '%d' "$3")
	EDX=$(printf '%d' "$4")
}

bit() { # bit VAL N  -> 1 if bit N of VAL set
	echo $(( ( $1 >> $2 ) & 1 ))
}

yesno() { [ "$1" -eq 1 ] && echo yes || echo no; }

hex32() { printf '0x%08x' "$1"; }

# ---- vendor + family/model ---------------------------------------------------

read_cpuid 0x00000000
# vendor string is EBX EDX ECX as 12 ASCII chars
vendor=$(printf '%08x%08x%08x' "$EBX" "$EDX" "$ECX" | \
	awk 'BEGIN { for (i = 0; i < 256; i++) h2d[sprintf("%02x", i)] = i }
	{
		s = "";
		for (i = 1; i <= length($0); i += 2) {
			h = substr($0, i, 2);
			s = s sprintf("%c", h2d[tolower(h)]);
		}
		# bytes are little-endian per register; flip each 4-byte group
		out = "";
		for (g = 1; g <= length(s); g += 4) {
			seg = substr(s, g, 4);
			for (j = 4; j >= 1; j--)
				out = out substr(seg, j, 1);
		}
		print out;
	}')

read_cpuid 0x00000001
sig=$EAX
base_family=$(( (sig >> 8)  & 0xF ))
base_model=$(( (sig >> 4)  & 0xF ))
stepping=$((  sig         & 0xF ))
ext_family=$(( (sig >> 20) & 0xFF ))
ext_model=$((  (sig >> 16) & 0xF ))

if [ "$base_family" -eq 15 ]; then
	family=$(( base_family + ext_family ))
else
	family=$base_family
fi

if [ "$base_family" -ge 6 ]; then
	model=$(( (ext_model << 4) | base_model ))
else
	model=$base_model
fi

# ---- Zen generation + codename (best-effort) ---------------------------------
#
# Mapping built from AMD PPRs and public roadmap leaks; entries with no public
# codename print "unknown".  The Zen6 entry is provisional.
#
zen_gen="non-Zen"
codename="unknown"

if [ "$vendor" = "AuthenticAMD" ]; then
	case $family in
	23) # 17h
		case $model in
		0x00|0x01|0x02|0x03|0x04|0x05|0x06|0x07|0x08|0x09|0x0A|0x0B|0x0C|0x0D|0x0E|0x0F)
			zen_gen="Zen 1"; codename="Naples / Summit Ridge" ;;
		17|24)  zen_gen="Zen 1";   codename="Raven Ridge / Snowy Owl" ;;
		8|24)   zen_gen="Zen+";    codename="Pinnacle Ridge / Colfax" ;;
		49|96|113|144|160) zen_gen="Zen 2";   codename="Rome / Matisse / Renoir" ;;
		*)
			if   [ $model -ge 0   ] && [ $model -le 15  ]; then zen_gen="Zen 1"; codename="Naples / Summit Ridge"
			elif [ $model -ge 16  ] && [ $model -le 31  ]; then zen_gen="Zen+/Zen APU"; codename="Raven Ridge"
			elif [ $model -ge 32  ] && [ $model -le 47  ]; then zen_gen="Zen+";  codename="Pinnacle Ridge"
			elif [ $model -ge 48  ] && [ $model -le 63  ]; then zen_gen="Zen 2"; codename="Castle Peak / Rome"
			elif [ $model -ge 64  ] && [ $model -le 79  ]; then zen_gen="Zen 2"; codename="Renoir"
			elif [ $model -ge 96  ] && [ $model -le 111 ]; then zen_gen="Zen 2"; codename="Renoir / Lucienne"
			elif [ $model -ge 112 ] && [ $model -le 127 ]; then zen_gen="Zen 2"; codename="Matisse"
			elif [ $model -ge 144 ] && [ $model -le 159 ]; then zen_gen="Zen 2"; codename="Van Gogh"
			elif [ $model -ge 160 ] && [ $model -le 175 ]; then zen_gen="Zen 2"; codename="Mendocino"
			fi ;;
		esac ;;
	25) # 19h
		if   [ $model -ge 0   ] && [ $model -le 15  ]; then zen_gen="Zen 3"; codename="Milan / EPYC 7003"
		elif [ $model -ge 16  ] && [ $model -le 31  ]; then zen_gen="Zen 4"; codename="Genoa / EPYC 9004"
		elif [ $model -ge 32  ] && [ $model -le 47  ]; then zen_gen="Zen 3"; codename="Vermeer / Ryzen 5000"
		elif [ $model -ge 48  ] && [ $model -le 63  ]; then zen_gen="Zen 3"; codename="Milan-X"
		elif [ $model -ge 64  ] && [ $model -le 79  ]; then zen_gen="Zen 3+"; codename="Rembrandt"
		elif [ $model -ge 80  ] && [ $model -le 95  ]; then zen_gen="Zen 3"; codename="Cezanne / Barcelo"
		elif [ $model -ge 96  ] && [ $model -le 111 ]; then zen_gen="Zen 4"; codename="Raphael / Dragon Range"
		elif [ $model -ge 112 ] && [ $model -le 127 ]; then zen_gen="Zen 4"; codename="Phoenix / Hawk Point"
		elif [ $model -ge 128 ] && [ $model -le 143 ]; then zen_gen="Zen 4"; codename="Storm Peak (Threadripper 7000)"
		elif [ $model -ge 160 ] && [ $model -le 175 ]; then zen_gen="Zen 4c"; codename="Bergamo / Siena"
		fi ;;
	26) # 1Ah
		if   [ $model -ge 0   ] && [ $model -le 15  ]; then zen_gen="Zen 5"; codename="Turin (EPYC 9005)"
		elif [ $model -ge 16  ] && [ $model -le 31  ]; then zen_gen="Zen 5c"; codename="Turin Dense"
		elif [ $model -ge 32  ] && [ $model -le 47  ]; then zen_gen="Zen 5"; codename="Strix Point / Strix Halo"
		elif [ $model -ge 64  ] && [ $model -le 79  ]; then zen_gen="Zen 5"; codename="Granite Ridge (Ryzen 9000)"
		elif [ $model -ge 80  ] && [ $model -le 95  ]; then zen_gen="Zen 5"; codename="Krackan Point"
		elif [ $model -ge 96  ] && [ $model -le 127 ]; then zen_gen="Zen 5/6 (provisional)"; codename="Strix Halo / Fire Range"
		elif [ $model -ge 144 ] && [ $model -le 175 ]; then zen_gen="Zen 6 (provisional)"; codename="Venice / Olympic Ridge"
		fi ;;
	esac
fi

# ---- print summary -----------------------------------------------------------

cat <<EOF
================================================================================
  AMD CPU Identification (CPU $CPU)
================================================================================
  Vendor       : $vendor
  CPUID sig    : $(hex32 $sig)
  Family       : 0x$(printf '%02x' $family) ($family)
  Model        : 0x$(printf '%02x' $model) ($model)
  Stepping     : $stepping
  Generation   : $zen_gen
  Codename     : $codename
EOF

# brand string (CPUID 0x80000002..0x80000004)
brand=""
for leaf in 0x80000002 0x80000003 0x80000004; do
	read_cpuid $leaf
	for reg in $EAX $EBX $ECX $EDX; do
		brand="$brand$(printf '%08x' $reg | \
			awk 'BEGIN { for (i = 0; i < 256; i++) h2d[sprintf("%02x", i)] = i }
			{
				out = "";
				for (i = 1; i <= length($0); i += 2) {
					h = substr($0, i, 2);
					out = out sprintf("%c", h2d[tolower(h)]);
				}
				flip = "";
				for (j = length(out); j >= 1; j--)
					flip = flip substr(out, j, 1);
				printf "%s", flip;
			}')"
	done
done
echo "  Brand        : $(echo "$brand" | sed 's/  */ /g; s/^ //; s/ $//')"
echo

# ---- IBS features (CPUID Fn8000_001B EAX) ------------------------------------

read_cpuid 0x8000001b
ibs_eax=$EAX

cat <<EOF
================================================================================
  IBS Features (CPUID Fn8000_001B)
================================================================================
  Raw EAX      : $(hex32 $ibs_eax)

  bit  feature                            present
  ---  ---------------------------------  -------
  00   IbsFfv (feature flags valid)         $(yesno $(bit $ibs_eax 0))
  01   IbsFetchSam                          $(yesno $(bit $ibs_eax 1))
  02   IbsOpSam                             $(yesno $(bit $ibs_eax 2))
  03   IbsRdWrOpCnt                         $(yesno $(bit $ibs_eax 3))
  04   IbsOpCnt                             $(yesno $(bit $ibs_eax 4))
  05   IbsBrnTrgt                           $(yesno $(bit $ibs_eax 5))
  06   IbsOpCntExt                          $(yesno $(bit $ibs_eax 6))
  07   IbsRipInvalidChk                     $(yesno $(bit $ibs_eax 7))
  08   IbsOpBrnFuse                         $(yesno $(bit $ibs_eax 8))
  09   IbsFetchCtlExtd                      $(yesno $(bit $ibs_eax 9))
  10   IbsOpData4                           $(yesno $(bit $ibs_eax 10))
  11   Zen4 IBS extensions                  $(yesno $(bit $ibs_eax 11))
  12   IbsLoadLatencyFilt                   $(yesno $(bit $ibs_eax 12))
  13   IbsDis (alt disable, Zen6 marker)    $(yesno $(bit $ibs_eax 13))
  14   IbsFetchLatencyFiltering             $(yesno $(bit $ibs_eax 14))
  15   IbsAddrBit63Filtering                $(yesno $(bit $ibs_eax 15))
  16   IbsStrmStAndRmtSocket                $(yesno $(bit $ibs_eax 16))
  17   IbsBufferV1                          $(yesno $(bit $ibs_eax 17))
  18   IbsMemProfilerV1                     $(yesno $(bit $ibs_eax 18))
  19   IbsUpdtdDtlbStats                    $(yesno $(bit $ibs_eax 19))
EOF

zen6=$(bit $ibs_eax 13)
echo
if [ "$zen6" -eq 1 ]; then
	echo "  -> CPU advertises the Zen6 IBS marker (IbsDis)."
else
	echo "  -> Pre-Zen6 CPU.  Zen6-only qualifiers (fetchlat=, addr63=,"
	echo "     streamstore) and the alternate disable path will not be"
	echo "     exercised on this host."
fi

# ---- usable pmcstat qualifiers ----------------------------------------------

cat <<EOF

================================================================================
  pmcstat IBS qualifiers usable on this host
================================================================================
EOF
ldlat=$(bit $ibs_eax 12)
fetchlat=$(bit $ibs_eax 14)
addr63=$(bit $ibs_eax 15)
strmst=$(bit $ibs_eax 16)
opcnt=$(bit $ibs_eax 4)

printf "  ibs-fetch:\n"
printf "    l3miss      : always (kernel-side, no CPUID gate)\n"
printf "    randomize   : always\n"
printf "    fetchlat=N  : %s\n" "$([ $fetchlat -eq 1 ] && echo yes || echo no)"
printf "    addr63=B    : %s\n" "$([ $addr63   -eq 1 ] && echo yes || echo no)"
printf "  ibs-op:\n"
printf "    l3miss      : always\n"
printf "    opcount     : %s\n" "$([ $opcnt    -eq 1 ] && echo yes || echo no)"
printf "    ldlat=N     : %s%s\n" \
	"$([ $ldlat -eq 1 ] && echo yes || echo no)" \
	"$([ $ldlat -eq 1 ] && [ $zen6 -eq 0 ] && echo " (must pair with l3miss on pre-Zen6)" || echo "")"
printf "    addr63=B    : %s\n" "$([ $addr63 -eq 1 ] && echo yes || echo no)"
printf "    streamstore : %s\n" "$([ $strmst -eq 1 ] && echo yes || echo no)"

# ---- hwpmc IBS sysctl state --------------------------------------------------

_ibs_sysctls=$(sysctl -N kern.hwpmc 2>/dev/null | grep ibs)
if [ -n "$_ibs_sysctls" ]; then
	cat <<EOF

================================================================================
  hwpmc IBS sysctl state
================================================================================
EOF
	for _s in $_ibs_sysctls; do
		sysctl "$_s"
	done
else
	cat <<EOF

================================================================================
  hwpmc not loaded (no kern.hwpmc.ibs sysctls)
================================================================================
  Run:  sudo kldload hwpmc
EOF
fi

