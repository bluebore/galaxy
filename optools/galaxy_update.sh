#!/bin/bash

function help() {
    echo -e "\nUsage: sh $0" 
    echo "-c smallPlus/bigPlus: smallPlus means replica of small json rised one by one; bigPlus means replica of big json rised one by one"
    echo "-r small replica: must be a number"
    echo "-R big replica: must be a number"
    echo "-s step: wait until for running pods num reaching to total replica when replica changed to the value; must be a number"
    echo "-j small job id (not needed if small json is submited firstly)"
    echo "-J big job id"
    echo "-f small json file"
    echo "-F big json file"
}

while getopts "c:s:t:j:J:f:F:r:R:h" opt
do
    case "$opt" in
        "c")
        cmd=$OPTARG
        ;;
        "s")
        step=$OPTARG
        ;;
        "r")
        small_replica=$OPTARG
        ;;
        "R")
        big_replica=$OPTARG
        ;;
        "j")
        small_jobId=$OPTARG
        ;;
        "J")
        big_jobId=$OPTARG
        ;;
        "f")
        small_json_file=$OPTARG
        ;;
        "F")
        big_json_file=$OPTARG
        ;;
        "h")
        help
        exit 1
        ;;

    esac
done

if [ "${cmd}" != "smallPlus" -a "${cmd}" != "bigPlus" ];then
    msg="-c param must be smallPlus or bigPlus"
    echo ${msg} >&2
    help
    exit 1
fi

if [ -z "${small_json_file}" ] ||  [ -z "${big_json_file}" ] || [ ! -s ${small_json_file} ] || [ ! -s ${big_json_file} ];then
    msg="-f [${small_json_file}] or -F [${big_json_file}] must be existed"
    echo ${msg} >&2
    help
    exit 1
fi

json_replica=`cat ${big_json_file} | awk -F ',' '{for(i=1;i<NF;i++)if($i~/replica/)print $i}' | awk -F ':' '{print $NF}'`
if [ -z ${json_replica} ] || [ ${json_replica} -ne 0 ];then
    msg="replica in json file must be 0"
    echo ${msg} >&2
    exit 1
fi
json_replica=`cat ${small_json_file} | awk -F ',' '{for(i=1;i<NF;i++)if($i~/replica/)print $i}' | awk -F ':' '{print $NF}'`
if [ -z ${json_replica} ] || [ ${json_replica} -ne 0 ];then
    msg="replica in json file must be 0"
    echo ${msg} >&2
    exit 1
fi


if [ -z ${big_jobId} ] || [ -z ${step} ] || [[ ! "${step}" =~ "^[0-9]+$" ]] || [ -z ${small_replica} ] || [[ ! "${small_replica}" =~ "^[0-9]+$" ]] || [ -z ${big_replica} ] || [[ ! "${big_replica}" =~ "^[0-9]+$" ]];then
    msg="-J must be not empty or -s and -r and -R must be number" 
    echo "${msg}" >&2
    help
    exit 1
fi

./galaxy jobs | grep ${big_jobId}
if [ $? -ne 0 ];then
    msg="big_jobId [${big_jobId}]is not existed";
    echo ${msg} >&2
    exit 1
fi

total_replica=`expr ${big_replica} + ${small_replica}`

already_big_replica=`./galaxy jobs | grep ${big_jobId} | awk '{print $6}'`
if [ -z ${already_big_replica} ] || [[ ! "${already_big_replica}" =~ "^[0-9]+$" ]];then
    msg="get big_jobId [${big_jobId}] replica  failed";
    echo ${msg} >&2
    exit -1;
fi

already_small_replica=0
if [ -n "${small_jobId}" ];then
    ./galaxy jobs | grep ${small_jobId}
    if [ $? -ne 0 ];then
        msg="small_jobId [${small_jobId}]is not existed";
        echo ${msg} >&2
        exit 1
    fi
    already_small_replica=`./galaxy jobs | grep ${small_jobId} | awk '{print $6}'`
    if [ -z ${already_small_replica} ] || [[ ! "${already_small_replica}" =~ "^[0-9]+$" ]];then
        msg="get small_jobId [${small_jobId}] replica  failed";
        echo ${msg} >&2
        exit -1;
    fi

    already_replica=`expr ${already_small_replica} + ${already_big_replica}`
    if [ ${total_replica} -lt ${already_replica} ];then
        msg="total_replica:${total_replica} is less than or equal to already_replica:${already_replica}";
        echo ${msg} >&2
        exit -1;
    fi
fi

cp ${small_json_file} $$_${small_json_file}
cp ${big_json_file} $$_${big_json_file}

if [ ${cmd} == "smallPlus" ];then

    if [ ${already_big_replica} -le ${big_replica} ];then
        msg="replica of ${big_jobId} is less than or equal to ${big_replica}";
        echo ${msg} >&2
        exit -1
    fi

    #temp_small_replica=${small_replica}
    if [ -n "${small_jobId}" ];then #small jobid提供
        if [ ${already_small_replica} -ge ${small_replica} ];then
            msg="replica of ${small_jobId} is larger than or equal to ${small_replica}";
            echo ${msg} >&2
            exit -1;
        fi

        big_change_count=`expr ${already_big_replica} -  ${big_replica}`
        small_change_count=`expr ${small_replica} - ${already_small_replica}`

        if [ ${big_change_count} -ne ${small_change_count} ];then
            msg="big_change_count:${big_change_count} is not equal to small_change_count:${small_change_count}"
            echo ${msg} >&2
            exit -1;
        fi

        sed -i "s/replica\"\:0/replica\"\:${already_big_replica}/" $$_${big_json_file}
        sed -i "s/replica\"\:0/replica\"\:${already_small_replica}/" $$_${small_json_file}

        last_small_replica=${already_small_replica}
        change_count=${big_change_count}
    else
        last_small_replica=1
        sed -i "s/replica\"\:0/replica\"\:${last_small_replica}/" $$_${small_json_file}
        small_jobId=`./galaxy submit -f $$_${small_json_file} | awk '{print $NF}'`
        if [ -z "${small_jobId}" ];then
            msg="galaxy submit -f $$_${small_json_file} failed";
            echo ${msg} >&2
            exit -1;
        fi
        #大的减1
        sed -i "s/replica\"\:0/replica\"\:$((--already_big_replica))/" $$_${big_json_file}
        ./galaxy update -j ${big_jobId} -f $$_${big_json_file}
        if [ $? -ne 0 ];then
            msg="./galaxy update -j ${big_jobId} -f $$_${big_json_file} failed"
            echo ${msg} >&2
            exit -1
        fi
        change_count=${small_replica}
    fi
    
    t=0
    for ((i=1;i<=${change_count};i++)) 
    do
        ((++t))
        sed -i "s/replica\"\:$((last_small_replica))/replica\"\:$((++last_small_replica))/" $$_${small_json_file}
        ./galaxy update -j ${small_jobId} -f $$_${small_json_file}
        if [ $? -ne 0 ];then
            msg="./galaxy update -j ${small_jobId} -f $$_${small_json_file} failed"
            echo ${msg} >&2
            exit -1
        fi
                
        #大的减1
        sed -i "s/replica\"\:${already_big_replica}/replica\"\:$((--already_big_replica))/" $$_${big_json_file}
        ./galaxy update -j ${big_jobId} -f $$_${big_json_file}
        if [ $? -ne 0 ];then
            msg="./galaxy update -j ${big_jobId} -f $$_${big_json_file} failed"
            echo ${msg} >&2
            exit -1
        fi
                 
        sleep 3

        if [ ${t} -ge ${step} ];then
            t=0;
            big_running_pods=`./galaxy jobs | grep ${big_jobId} | awk '{print $5}' | awk -F '/' '{print $1}'`
            small_running_pods=`./galaxy jobs | grep ${small_jobId} | awk '{print $5}' | awk -F '/' '{print $1}'`
            if [ -z "${big_running_pods}" ] || [[ ! "${big_running_pods}" =~ "^[0-9]+$" ]] || [ -z "${small_running_pods}" ] || [[ ! ${small_running_pods} =~ "^[0-9]+$" ]];then
                msg="get running pods number failed"
                echo ${msg} >&2
                exit -1
            fi

            running_pods=`expr ${big_running_pods} + ${small_running_pods}`
            while [ ${running_pods} -lt ${total_replica} ] 
            do
                big_running_pods=`./galaxy jobs | grep ${big_jobId} | awk '{print $5}' | awk -F '/' '{print $2}'`
                small_running_pods=`./galaxy jobs | grep ${small_jobId} | awk '{print $5}' | awk -F '/' '{print $2}'`
                if [ -z "${big_running_pods}" ] || [[ ! ${big_running_pods} =~ "^[0-9]+$" ]] || [ -z "${small_running_pods}" ] || [[ ! ${small_running_pods} =~ "^[0-9]+$" ]];then
                    msg="get running pods number failed"
                    echo ${msg} >&2
                    exit -1
                fi
                running_pods=`expr ${big_running_pods} + ${small_running_pods}`
                sleep 10
            done
        fi
    done
elif [ ${cmd} == "bigPlus" ];then
    if [ -z "${small_jobId}" ];then #没small jobid提供
        msg="-j:small_jobId is needed"
        echo "$msg" >&2
        exit 1
    fi

    if [ ${already_small_replica} -le ${small_replica} ];then
        msg="replica of ${small_jobId} is less than or equal to ${small_replica}";
        echo ${msg} >&2
        exit -1;
    fi
    
    if [ ${already_big_replica} -ge ${big_replica} ];then
        msg="replica of ${big_jobId} is larger than or equal to ${big_replica}";
        echo ${msg} >&2
        exit -1;
    fi

    big_change_count=`expr ${big_replica} - ${already_big_replica}`
    small_change_count=`expr ${already_small_replica} - ${small_replica}`

    if [ ${big_change_count} -ne ${small_change_count} ];then
        msg="big_change_count:${big_change_count} is not equal to small_change_count:${small_change_count}"
        echo ${msg} >&2
        exit -1;
    fi

    sed -i "s/replica\"\:0/replica\"\:${already_small_replica}/" $$_${small_json_file}
    sed -i "s/replica\"\:0/replica\"\:${already_big_replica}/" $$_${big_json_file}

    change_count=${big_change_count}
    t=0
    for ((i=1;i<=${change_count};i++)) 
    do
        ((++t))
        sed -i "s/replica\"\:$((already_small_replica))/replica\"\:$((--already_small_replica))/" $$_${small_json_file}
        ./galaxy update -j ${small_jobId} -f $$_${small_json_file}
        if [ $? -ne 0 ];then
            msg="./galaxy update -j ${small_jobId} -f $$_${small_json_file} failed"
            echo ${msg} >&2
            exit -1;
        fi
                
        #大的减1
        sed -i "s/replica\"\:${already_big_replica}/replica\"\:$((++already_big_replica))/" $$_${big_json_file}
        ./galaxy update -j ${big_jobId} -f $$_${big_json_file}
        if [ $? -ne 0 ];then
            msg="./galaxy update -j ${big_jobId} -f $$_${big_json_file} failed"
            echo ${msg} >&2
            exit -1
        fi
                 
        sleep 3

        if [ ${t} -ge ${step} ];then
            t=0;
            big_running_pods=`./galaxy jobs | grep ${big_jobId} | awk '{print $5}' | awk -F '/' '{print $1}'`
            small_running_pods=`./galaxy jobs | grep ${small_jobId} | awk '{print $5}' | awk -F '/' '{print $1}'`
            if [ -z "${big_running_pods}" ] || [[ ! "${big_running_pods}" =~ "^[0-9]+$" ]] || [ -z "${small_running_pods}" ] || [[ ! ${small_running_pods} =~ "^[0-9]+$" ]];then
                msg="get running pods number failed"
                echo ${msg} >&2
                exit -1
            fi

            running_pods=`expr ${big_running_pods} + ${small_running_pods}`
            while [ ${running_pods} -lt ${total_replica} ] 
            do
                big_running_pods=`./galaxy jobs | grep ${big_jobId} | awk '{print $5}' | awk -F '/' '{print $2}'`
                small_running_pods=`./galaxy jobs | grep ${small_jobId} | awk '{print $5}' | awk -F '/' '{print $2}'`
                if [ -z "${big_running_pods}" ] || [[ ! ${big_running_pods} =~ "^[0-9]+$" ]] || [ -z "${small_running_pods}" ] || [[ ! ${small_running_pods} =~ "^[0-9]+$" ]];then
                    msg="get running pods number failed"
                    echo ${msg} >&2
                    exit -1
                fi
                running_pods=`expr ${big_running_pods} + ${small_running_pods}`
                sleep 10
            done
        fi
    done

else
    msg="-c param error";
    echo ${msg} >&2
fi

rm $$_${small_json_file}
rm $$_${big_json_file}

exit 0
