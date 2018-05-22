#! /bin/sh

DEBUG=0

INIT=0
BOOTSTRAP=
REDUNDANT_IDX=
IBC=
JCI_IMG=
KERNEL1=
KERNEL2=
ROOTFS=
EMMC=

KERNEL_VOL_SZ=5800
ROOTFS_VOL_SZ=180
JCI_VOL_SZ=320
DATA_VOL_SZ=80
PERS_VOL_SZ=64
CACHE_VOL_SZ=110

print_usage()
{
	echo "Usage:"
	echo "configure_flash.sh --init --bootstrap=img --ibc=img --jci[=img] --kernel=img --fskernel=img --rootfs=img -emmc=img -d"
	echo ""
	echo "Where:"
	echo "  --init will re-initialize ALL UBI and MTD partitions (ALL DATA WILL BE LOST)"
	echo "  'img' is the image to be programmed to the device"
	echo ""
}

dbg_exec()
{
	if [[ "${DEBUG}" == "1" ]]; then
		echo $@
	else
		eval "$@"
	fi
}

waitfor() {

	local dev=$1
	local ii=60

	if [[ -n $2 ]]; then
		ii=$2
	fi

	let ii=$((ii*2))
	while [[ ! -e "${dev}" && $ii -ne 0 ]]; do

		let ii=$((ii-1))

		sleep 0.5

	done
}

################################################################################
# Read and parse command line options
################################################################################
parse_command_line()
{
	local _opt=""
	local _optval=""
	local _arg=""
	local _junk=""

	for _arg in $@; do

		# Break option into '_opt=_optval' parts
		_junk=$(echo ${_arg} | grep -c =)
		if [[ "${_junk}" == "1" ]]; then
			_opt=${_arg%%=*}
			_optval=${_arg#*=}
		else
			_opt=${_arg}
			_optval=""
		fi

		case ${_opt} in
			--init )		INIT=1 ;;

			--bootstrap )	BOOTSTRAP=${_optval} ;;

			--ibc )			IBC=${_optval} ;;

			--jci )			JCI_IMG=${_optval} ;;

			--kernel )		KERNEL1=${_optval} ;;

			--fskernel )	KERNEL2=${_optval} ;;

			--rootfs )		ROOTFS=${_optval} ;;

			--emmc)			EMMC=${_optval} ;;

			-1 )			REDUNDANT_IDX="${REDUNDANT_IDX} 1" ;;

			-2 )			REDUNDANT_IDX="${REDUNDANT_IDX} 2" ;;

			--help )		print_usage
							exit 0 ;;

			-h )			print_usage
							exit 0 ;;

			-d )			DEBUG=1 ;;

			*)
				echo "Unrecognized: \"${_opt}\"" ;
				print_usage ;
				exit 1 ;;
		esac
	done

	# If no redundant image index was specified
	if [[ -z "${REDUNDANT_IDX}" ]]; then
		if [[ -n "${IBC}" ]]; then
			# Default to re-flashing both copies
			REDUNDANT_IDX="1 2"
		fi
	fi

	# Trim leading/trailing whitespace
	REDUNDANT_IDX=${REDUNDANT_IDX## }
	REDUNDANT_IDX=${REDUNDANT_IDX%% }


	if [[ "${DEBUG}" == "0" ]]; then
		# Verify that the images passed in are present
		for _img in "BOOTSTRAP" "IBC" "KERNEL" "ROOTFS" "JCI"; do
			eval _mtd_img=\$$_img

			if ! [[ -z "${_mtd_img}" || -f "${_mtd_img}" ]]; then
				echo "${_img} image '${_mtd_img}' does not exist"
				exit 1
			fi
		done
	fi

#	echo "         INIT='${INIT}'         "
#	echo "    BOOTSTRAP='${BOOTSTRAP}'    "
#	echo "REDUNDANT_IDX='${REDUNDANT_IDX}'"
#	echo "          IBC='${IBC}'          "
#	echo "       KERNEL='${KERNEL}'       "
#	echo "       ROOTFS='${ROOTFS}'       "
#	exit 0
}

setup_datalight_env() 
{
	local ffxos="ffxos/ffxos.ko"
	local flashfx="flashfx/flashfx.ko"
	local ffxblk="ffxblk/ffxblk.ko"
	local relos="relos/relos.ko"
	local reliance="reliance/reliance.ko"
	local relfs="relfs/relfs.ko"
	local mod_list="${ffxos} ${flashfx} ${ffxblk} ${relos}  ${reliance} ${relfs}"

	mkdir -p /lib/modules/3.0.35/kernel/drivers/datalight
	mkdir -p /sbin

	# Extract the modules
	tar -zxf /tmp/flashfx_modules.tar.gz  -C /lib/modules/3.0.35/kernel/drivers/datalight
	tar -zxf /tmp/reliance-nitro_modules.tar.gz  -C /lib/modules/3.0.35/kernel/drivers/datalight

	# Extract the tools
	tar -zxf /tmp/flashfx_tools.tar.gz  -C /sbin
	tar -zxf /tmp/reliance-nitro_tools.tar.gz  -C /sbin

	for mod in ${mod_list}; do
		echo "Load kernel module: ${mod}"
		dbg_exec insmod /lib/modules/3.0.35/kernel/drivers/datalight/${mod}
	done
	sleep 1
}

cleanup_datalight_env() 
{
	echo "Unmount file systems"
	dbg_exec umount -f /dev/ffx01p* &>/dev/null
	sleep 1
		
	echo "Unload ffxblk kernel module" 
	dbg_exec rmmod ffxblk
}

################################################################################
# Reads the size of the partition using the output of 'mtdinfo'
# Inputs:
#	$1: The MTD device whose size we want to know (i.e. /dev/mtd3)
# Return Vars:
#	MTD_SIZE
#	MTD_BLOCK_CNT
#	MTD_BLOCK_SZ
################################################################################
get_mtd_sz()
{
	local _mtd_dev=$1
	local _mtd_info=$(mtdinfo ${_mtd_dev} | grep "Amount of eraseblocks" | sed 's/[\(\) ]\+/ /g')
	local _status="$?"

	# If the 'mtdinfo' call failed
	if [[ "${_status}" != "0" ]]; then
		if [[ "${DEBUG}" == "1" ]]; then
			MTD_SIZE=$((1024*1024))
			MTD_BLOCK_CNT=8
			MTD_BLOCK_SZ=$((MTD_SIZE / MTD_BLOCK_CNT))
		else
			echo "ERROR: mtdinfo failed with status ${_status}"
			exit 1
		fi
	else
		MTD_SIZE=$(echo ${_mtd_info} | cut -d' ' -f5)
		MTD_BLOCK_CNT=$(echo ${_mtd_info} | cut -d' ' -f4)
		MTD_BLOCK_SZ=$((MTD_SIZE / MTD_BLOCK_CNT))
	fi

}

################################################################################
# Reads the type of the partition using the output of 'mtdinfo' (nor or nand)
# Inputs:
#	$1: The MTD device whose type we want to know (i.e. /dev/mtd3)
# Return Vars:
#	MTD_TYPE:	nor or nand
################################################################################
get_mtd_type()
{
	local _mtd_dev=$1
	local _mtd_type=$(mtdinfo ${_mtd_dev} | grep ^Type | sed 's/[\( ]\+/ /g' | cut -d' ' -f2)
	local _status="$?"

	# If the 'mtdinfo' call failed
	if [[ "${_status}" != "0" ]]; then
		if [[ "${DEBUG}" == "1" ]]; then
			MTD_TYPE=nor
		else
			echo "ERROR: mtdinfo failed with status ${_status}"
			exit 1
		fi
	else
		MTD_TYPE="${_mtd_type}"
	fi
}

################################################################################
# Read the name of the partition using the output of 'mtdinfo'
# Inputs:
#	$1: The MTD device whose name we want to know (i.e. /dev/mtd3)
# Return Vars:
#	MTD_NAME
################################################################################
get_mtd_name()
{
	local _mtd_dev=${1##/dev/}
	local _mtd_name=$(cat /sys/class/mtd/${_mtd_dev}/name)
	local _status="$?"

	# If the 'mtdinfo' call failed
	if [[ "${_status}" != "0" ]]; then
		if [[ "${DEBUG}" == "1" ]]; then
			MTD_NAME=debug
		else
			echo "ERROR: mtdinfo failed with status ${_status}"
			exit 1
		fi
	else
		MTD_NAME="${_mtd_name}"
	fi
}

################################################################################
# Searches for an MTD device with the specified name (i.e. bootstrap)
# Inputs:
#	$1: The name of the MTD partition we are looking for (i.e. bootstrap)
# Return Vars:
#	MTD_DEV: /dev/mtdX
################################################################################
find_mtd_by_name()
{
	local _mtd_dev=$(find /sys/class/mtd -follow -maxdepth 2 -path '/sys/class/mtd/mtd[[:digit:]]*/name' | xargs grep -l ^"$1"$)
	local _status="$?"

	MTD_DEV=""
	# If the 'find' call failed
	if [[ "${_status}" == "0" && -n "${_mtd_dev}" ]]; then
		_mtd_dev=${_mtd_dev%%/name*}
		if [[ -n "${_mtd_dev}" ]]; then
			#echo "MTD_DEV: ${_mtd_dev}"
			MTD_DEV=/dev/${_mtd_dev##*/}
			#echo "MTD_DEV: ${MTD_DEV}"
		fi
	fi

	if [[ -z "${MTD_DEV}" ]]; then
		if [[ "${DEBUG}" == 1 ]]; then
			MTD_DEV=/dev/mtd27
		else
			echo "ERROR: find_mtd_by_name failed with status ${_status} for '${1}'"
			exit ${_status}
		fi
	fi
}

################################################################################
# Re-initialize a block device
# Inputs:
#	$1: The name of the block device to initialize for RelFS usage
################################################################################
relfs_init()
{
	local _ffx_dev=${1}

	# Remove all existing partitions
	echo "================================================================================"
	echo "Delete existing partitions on ${_ffx_dev}"
	echo "--------------------------------------------------------------------------------"
	#dbg_exec printf "d\\n1\\nd\\n2\\nd\\n3\\nd\\n4\\nw\\n" | fdisk ${_ffx_dev}
	dbg_exec fdisk ${_ffx_dev} << __EOF
d
1
d
2
d
3
d
w
__EOF

	echo
	echo "--------------------------------------------------------------------------------"
	
	sleep 1
	echo "Create new partitions on ${_ffx_dev}"
	#dbg_exec printf "n\\np\\n1\\n\\n+500MB\\nn\\np\\n2\\n\\n+64M\\nn\\np\\n3\\n\\n+64M\\nn\\np\\n\\n\\nw\\n" | fdisk ${_ffx_dev}
	dbg_exec fdisk ${_ffx_dev} << __EOF
n
p
1

+500MB
n
p
2

+64M
n
p
3

+64M
n
p


w
__EOF

	# format all partitions without rootfs (p1)
	for IDX in 2 3 4; do
		if [ -b ${_ffx_dev}p${IDX} ]; then
			echo "--------------------------------------------------------------------------------"
			echo "Format partition '${_ffx_dev}p${IDX}'"
			dbg_exec /sbin/mkfs.relfs ${_ffx_dev}p${IDX} -NoConfirm
		fi
	done

	echo "--------------------------------------------------------------------------------"
	dbg_exec fdisk -l ${_ffx_dev}
	echo "================================================================================"
}

################################################################################
# Configures the boot-select partition to boot to a specific redundant image
# Inputs:
#	$1: The redundant image to select (1 or 2)
# Return Vars:
#	None
################################################################################
switch_ibc()
{
	local _ibc_idx=$1
	local _mtd_name="boot-select"

	echo
	echo "================================================================================"
	echo "Switching to IBC ${_ibc_idx}"
	echo "================================================================================"

	# Try to find an MTD device with the specified name
	find_mtd_by_name ${_mtd_name}

	# Does the desired MTD device exist
	if [[ -n "${MTD_DEV}" && -e "${MTD_DEV}" || "${DEBUG}" == "1" ]]; then

		# Get the size, block count and block size of the partition
		get_mtd_sz ${MTD_DEV}

		# Check for NOR or NAND
		get_mtd_type ${MTD_DEV}

		if [[ "${MTD_TYPE}" != "nor" ]]; then
			echo "ERROR: MTD deivce '${MTD_DEV}' for '${_mtd_name}' is '${MTD_TYPE}'- not nor!"
			exit 1
		fi

		# check argument
		if [[ "${_ibc_idx}" == "1" ]]; then
			dbg_exec /usr/sbin/flash_erase ${MTD_DEV} 0 0 2> /dev/null
		else
			dbg_exec dd if=/dev/zero of=${MTD_DEV} bs=1 count=1 2> /dev/null
		fi
		echo "================================================================================"
		echo "Switched to IBC ${_ibc_idx}"
		echo "================================================================================"
		echo

	else
		echo "ERROR: Cant find MTD device for '${MTD_NAME}': ${MTD_DEV}"
		exit 1
	fi
}

################################################################################
# Reflash a raw MTD partition
# Inputs:
#	$1: MTD partition name
#	$2: MTD image name
#	$3: Which redundant image is being programmed
# Return Vars:
#
################################################################################
flash_mtd()
{
	local _mtd_name="${1}${3}"
	local _mtd_img="${2}"
	local _idx="${3}"

	# Clear variables
	MTD_NAME=
	MTD_DEV=
	MTD_SIZE=

	# Do we have a valid MTD partition to work on
	if [[ -n "${_mtd_name}" ]]; then

		echo "--------------------------------------------------------------------------------"

		# Do we have a partition name for this image
		if [[ -n "${_mtd_name}" ]]; then
			# Try to find an MTD device with the specified name
			find_mtd_by_name "${_mtd_name}"
			if [[ -z "${MTD_DEV}" ]]; then
				echo "Can't find partition named '${_mtd_name}' for '${_mtd_img}'"
				continue
			fi
		fi

		# Does the desired MTD device exist
		if [[ -n "${MTD_DEV}" && -e "${MTD_DEV}" || "${DEBUG}" == "1" ]]; then

			# Get the size, block count and block size of the partition
			get_mtd_sz ${MTD_DEV}

			# Check for NOR or NAND
			get_mtd_type ${MTD_DEV}

			echo "Re-programming ${MTD_TYPE} partition '${_mtd_name}' (${MTD_DEV}, ${MTD_SIZE} bytes) from image '${_mtd_img}':"
			case ${MTD_TYPE} in

				# Erase NOR using /usr/sbin/flash_erase ${MTD_DEV} 0 0
				# Program raw NOR using dd if=${_mtd_img} of=${MTD_DEV} bs=${MTD_BLOCK_SZ} count=${MTD_BLOCK_CNT}
				nor)	echo "  Erasing '${_mtd_name}'"
						#dbg_exec /usr/sbin/flash_erase ${MTD_DEV} 0 0 ;
						dbg_exec /usr/sbin/mtd_debug erase ${MTD_DEV} 0 ${MTD_SIZE} ;
						echo "  Programming '${_mtd_name}'"
						dbg_exec dd if=${_mtd_img} of=${MTD_DEV} bs=${MTD_BLOCK_SZ} count=${MTD_BLOCK_CNT} ;
						;;

				# Erase NAND using /usr/sbin/flash_erase ${MTD_DEV} 0 0
				# Program raw NAND using /usr/sbin/nandwrite -m -p -s 0x0000 ${MTD_DEV} ${_mtd_img}
				nand)	echo "  Erasing '${_mtd_name}'"
						dbg_exec /usr/sbin/flash_erase ${MTD_DEV} 0 0 ;
						echo "  Programming '${_mtd_name}'"
						dbg_exec /usr/sbin/nandwrite -m -p -s 0x0000 ${MTD_DEV} ${_mtd_img} ;
						;;

				*)		echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" ;
						echo "SKIPPING Unknown partition type: ${MTD_TYPE}"
						echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" ;
						;;
			esac
			echo "'${_mtd_name}' re-programmed"

		else
			echo "Cant find MTD device for '${_mtd_name}' (${MTD_DEV})"
		fi
		echo "--------------------------------------------------------------------------------"
		echo
	fi
}

################################################################################
# Reflash a UBI file system volume
# Inputs:
#	$1: The "root" MTD partition name (i.e. 'rootfs' from 'rootfs1')
#	$2: Which redundant image is being programmed (1 or 2)
#	$3: Path to file system image file (or archive file)
#	$4: Size of the UBI volume (MiB)
#	$5: UBI device number to use (i.e. X from /dev/ubiX)
################################################################################
flash_relfs_part()
{
	# Read the MTD name and the image to program to it
	local _blk_dev="${1}"
	local _part_idx="${2}"
	local _img_path="${3}"
	local _part_name="${4}"
	local _mnt_point="/tmp/relfs/${4}"
	local _part_dev="${_blk_dev}p${_part_idx}"


	echo "--------------------------------------------------------------------------------"
	echo "Formatting filesystem '${_part_name}' from '${_img_path}'"
	dbg_exec /sbin/mkfs.relfs ${_part_dev} -NoConfirm
	
	# Mount the partition
	echo "Mounting device '${_part_dev}' at '${_mnt_point}'"
	dbg_exec mkdir -p ${_mnt_point}
	dbg_exec sleep 1
	dbg_exec mount -t relfs -o rw ${_part_dev} ${_mnt_point}
	dbg_exec sleep 1s

	# If we have an image to program
	if [[ -n "${_img_path}" ]]; then

		# Program with new root fs
		echo "Extract ${_img_path} to ${_mnt_point}"
		dbg_exec tar -xf ${_img_path} -C ${_mnt_point}
		
	fi
	
	# Remount the file system as read-only
	dbg_exec mount -o remount,ro ${_mnt_point}
}

################################################################################
# Flash a linux image to a UBI volume
# Inputs:
#	$1: Which redundant image is being programmed
#	$2: Filename of kernel image being programmed
# Return Vars:
#
################################################################################
flash_kernel_image() {

	local _blk_dev="/dev/ffx00"
	local _img_name="${1}"

	echo "Flashing linux image ${_img_name} to ${_blk_dev}"
	
	dbg_exec dd if=${_img_name} of=${_blk_dev} 

}

################################################################################
# SCRIPT STARTS HERE
################################################################################

# Don't let anything stupid happen to my laptop
if [[ "$(uname -m)" != "armv7l" ]]; then
	echo "FORCING DEBUG FOR NON-ARM TARGET"
	DEBUG=1
fi

# Read command line options
# parse_command_line $@

# Install the required kernel modules
# setup_datalight_env

switch_ibc 2

echo "Synchronizing changes"
dbg_exec sync

#cleanup_datalight_env

echo "Done"

exit 0
