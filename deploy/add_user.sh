#!/bin/bash
CPU_CFS_PERIOD=100000
MIN_CPU_CFS_QUOTA=1000
CG_ROOT="/cgroups"
valid_arg=0
user_name=""
cpu_cores=0
memory_gbytes=0
directoy_list=""

current_user=`id -u`
if [ $current_user -ne 0 ]; then
    echo "need root privilage"
    exit -1
fi

function print_help(){
    echo "./add_user.sh -u [user name] -c [cpu cores] -m [memory in GB] -d [direcotry list](optional)"
    echo "example:"
    echo "      ./add_user.sh -u abc -c 10 -m 3"
    echo "      ./add_user.sh -u nfs -c 2 -m 16 -d /home/disk0/nfsdata,/home/disk1/nfsdata"
}

while getopts "u:c:m:d:" opt
do
    case $opt in
    "u")
    user_name=$OPTARG
    ((valid_arg=$valid_arg+1))
    ;;

    "c")
    cpu_cores=$OPTARG
    ((valid_arg=$valid_arg+1))
    ;;

    "m")
    memory_gbytes=$OPTARG
    ((valid_arg=$valid_arg+1))
    ;;

    "d")
    directoy_list=$OPTARG 
    ;;
    esac
done

if [ $valid_arg -ne 3 -o "$user_name" == "" -o "$cpu_cores" == "" -o "$memory_gbytes" == "" ]; then
    print_help
    exit 1
fi

cpu_limit=`awk -v x=$cpu_cores -v y=$CPU_CFS_PERIOD 'BEGIN{print int(x*y)}'`
memory_limit=`awk -v x=$memory_gbytes 'BEGIN{print x*1024*1024*1024}'`
if [ $cpu_limit -lt $MIN_CPU_CFS_QUOTA ]; then
    cpu_limit=$MIN_CPU_CFS_QUOTA
fi

useradd $user_name
rule_str="${user_name} ALL=(ALL) NOPASSWD: /etc/profile.d/galaxy_${user_name}.sh"
grep -F "$rule_str"  /etc/sudoers
if [ $? -ne 0 ]; then
    echo  $rule_str >> /etc/sudoers
fi

if [ $? -ne 0 ]; then
    echo "faild to change sudoers"
    exit 3
fi
if [ "$directoy_list" != "" ]; then
    echo $directoy_list | awk -v RS="," '{print}' | while read dir_name
    do
        if [ ! -z $dir_name ]; then
            mkdir -p $dir_name
            if [ $? -ne 0 ]; then
                echo "faild to create dir: ${dir_name}"
                exit 4
            fi
            chown -R ${user_name}:${user_name} ${dir_name}
        fi
    done
fi

cat <<EOF > /etc/profile.d/galaxy_${user_name}.sh
if [ \$# -gt 0 -a "\$1" == "cg_limit" ]; then
    echo "Galaxy Limit Init ..." >&2
    mkdir -p $CG_ROOT/cpu/${user_name}
    mkdir -p $CG_ROOT/memory/${user_name}

    echo ${cpu_limit} > $CG_ROOT/cpu/${user_name}/cpu.cfs_quota_us
    if [ $? -ne 0 ]; then
        echo "set cpu limit fail" >&2
        exit 1
    fi
    echo ${memory_limit} > $CG_ROOT/memory/${user_name}/memory.limit_in_bytes
    if [ $? -ne 0 ]; then
        echo "set memory limit fail" >&2
        exit 2
    fi

    echo \$2 >> ${CG_ROOT}/cpu/${user_name}/tasks
    echo \$2 >> ${CG_ROOT}/memory/${user_name}/tasks
    exit 0
else
    if [ "\$USER" == "$user_name" ]; then
        echo "== Welcome to Galaxy ==" >&2
        sudo  /etc/profile.d/galaxy_${user_name}.sh cg_limit \$\$
        if [ \$? -ne 0 ]; then
            echo "Galaxy Init fail, exit" >&2
            exit 1
        fi
        echo "------------------------------------" >&2
        echo "CPU Cores:    |  ${cpu_cores}       " >&2
        echo "Memory Limit: |  ${memory_gbytes} GB" >&2
        if [ "$directoy_list" != "" ]; then
        echo "Directories:  |  ${directoy_list}   " >&2
        fi
        echo "------------------------------------" >&2
    fi
fi
EOF

if [ $? -ne 0 ]; then
    echo "faild to set hook script at profile.d"
    exit 4
fi

chmod 755 /etc/profile.d/galaxy_${user_name}.sh


echo "Done"


