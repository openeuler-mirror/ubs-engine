#!/bin/bash

#
# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
#

_ubse_mem_current_type() {
    local i last_t_idx=-1
    for ((i = COMP_CWORD - 1; i >= 2; i--)); do
        if [[ "${COMP_WORDS[i]}" == "-t" || "${COMP_WORDS[i]}" == "--type" ]]; then
            last_t_idx=$i
            break
        fi
    done
    if [[ ${last_t_idx} -ge 0 ]]; then
        local next_idx=$((last_t_idx + 1))
        if [[ ${next_idx} -lt ${COMP_CWORD} ]]; then
            echo "${COMP_WORDS[next_idx]}"
        fi
    fi
}

# process-mem: 补全运行中进程 PID（/proc 扫描）
_ubse_proc_pids() {
    compgen -W "$(ls /proc 2>/dev/null | grep -E '^[0-9]+$' | sort -n)" -- "$1"
}

# process-mem: 补全运行中进程名（/proc/<pid>/comm，去重）
_ubse_proc_names() {
    local names=""
    for name in /proc/[0-9]*/comm; do
        local comm
        read -r comm < "$name" 2>/dev/null
        [ -n "$comm" ] && names="$names $comm"
    done
    compgen -W "$(echo $names | tr ' ' '\n' | sort -u | tr '\n' ' ')" -- "$1"
}

function _ubse_commond_completion() {
    COMPREPLY=()

    local cur=${COMP_WORDS[COMP_CWORD]}
    local cmd=${COMP_WORDS[COMP_CWORD-1]}
    local prev=${COMP_WORDS[COMP_CWORD-2]}

commands='create display import delete check change remove detach attach'
    display_types='topo memory cluster node urma urma-qos process-mem'
    create_types='memory urma-qos'
    delete_types='memory urma-qos'
    check_types='memory'

    case "${cmd}" in
        *'ubsectl')
            COMPREPLY=( $(compgen -W "${commands}" -- $cur) )
            return 0
        ;;
        '*')
            return 0
        ;;
    esac

    if [[ "${prev}" == *ubsectl ]]; then
        case "${cmd}" in
            'display')
                COMPREPLY=( $(compgen -W "${display_types}" -- ${cur}) )
                return 0
            ;;
            'import')
                COMPREPLY=( $(compgen -W 'cert' -- ${cur}) )
                return 0
            ;;
            'change'|'remove')
                COMPREPLY=( $(compgen -W 'cert process-mem' -- ${cur}) )
                return 0
            ;;
            'check')
                COMPREPLY=( $(compgen -W "${check_types}" -- ${cur}) )
                return 0
            ;;
            'create')
                COMPREPLY=( $(compgen -W "${create_types}" -- ${cur}) )
                return 0
            ;;
            'delete')
                COMPREPLY=( $(compgen -W "${delete_types}" -- ${cur}) )
                return 0
            ;;
            'detach'|'attach')
                COMPREPLY=( $(compgen -W 'memory' -- ${cur}) )
                return 0
            ;;
            '*')
                return 0
            ;;
        esac
    fi

    if [[ "${cur}" == --* ]] ; then
        case ${COMP_WORDS[1]} in
            'display')
                case ${COMP_WORDS[2]} in
                    'memory')
                        if [[ "$(_ubse_mem_current_type)" == "numa_status" ]]; then
                            COMPREPLY=( $(compgen -W '--all' -- ${cur}) )
                            return 0
                        fi
                        COMPREPLY=( $(compgen -W '--type --borrow-type --name' -- ${cur}) )
                        return 0
                    ;;
                    'process-mem')
                        COMPREPLY=( $(compgen -W '--type' -- ${cur}) )
                        return 0
                    ;;
                    'topo')
                        COMPREPLY=( $(compgen -W '--type' -- ${cur}) )
                        return 0
                    ;;
                    'node')
                        COMPREPLY=( $(compgen -W '--node' -- ${cur}) )
                        return 0
                    ;;
                    'urma')
                        COMPREPLY=( $(compgen -W '--node --dev' -- ${cur}) )
                        return 0
                    ;;
                    'urma-qos')
                        COMPREPLY=( $(compgen -W '--node' -- ${cur}) )
                        return 0
                    ;;
                    '*')
                        return 0
                    ;;
                esac
            ;;

            'import')
                case ${COMP_WORDS[2]} in
                    'cert')
                        COMPREPLY=( $(compgen -W '--server-cert-file --ca-cert-file --server-key-file --ca-crl-file' -- ${cur}) )
                        return 0
                    ;;
                    '*')
                        return 0
                    ;;
                esac
            ;;

            'change')
                case ${COMP_WORDS[2]} in
                    'cert')
                        COMPREPLY=( $(compgen -W '--ca-crl-file' -- ${cur}) )
                        return 0
                    ;;
                    'process-mem')
                        COMPREPLY=( $(compgen -W '--pid --name --size --remote-ratio' -- ${cur}) )
                        return 0
                    ;;
                    '*')
                        return 0
                    ;;
                esac
            ;;

            'remove')
                case ${COMP_WORDS[2]} in
                    'process-mem')
                        COMPREPLY=( $(compgen -W '--pid --name' -- ${cur}) )
                        return 0
                    ;;
                    '*')
                        return 0
                    ;;
                esac
            ;;

            'create')
                case ${COMP_WORDS[2]} in
                    'memory')
                        COMPREPLY=( $(compgen -W '--type --link-id --size --name --region' -- ${cur}) )
                        return 0
                    ;;
                    'urma-qos')
                        COMPREPLY=( $(compgen -W '--pri --cir --node' -- ${cur}) )
                        return 0
                    ;;
                    '*')
                        return 0
                    ;;
                esac
            ;;

            'delete')
                case ${COMP_WORDS[2]} in
                    'memory')
                        COMPREPLY=( $(compgen -W '--name --type' -- ${cur}) )
                        return 0
                    ;;
                    'urma-qos')
                        COMPREPLY=( $(compgen -W '--node' -- ${cur}) )
                        return 0
                    ;;
                    '*')
                        return 0
                    ;;
                esac
            ;;

            'attach')
                case ${COMP_WORDS[2]} in
                    'memory')
                        COMPREPLY=( $(compgen -W '--name' -- ${cur}) )
                        return 0
                    ;;
                    '*')
                        return 0
                    ;;
                esac
            ;;

            'detach')
                case ${COMP_WORDS[2]} in
                    'memory')
                        COMPREPLY=( $(compgen -W '--name' -- ${cur}) )
                        return 0
                    ;;
                    '*')
                        return 0
                    ;;
                esac
            ;;
            '*')
                return 0
            ;;
        esac
        return 0

    elif [[ "${cur}" == -* ]]; then
        case ${COMP_WORDS[1]} in
            'display')
                case ${COMP_WORDS[2]} in
                    'memory')
                        if [[ "$(_ubse_mem_current_type)" == "numa_status" ]]; then
                            COMPREPLY=( $(compgen -W '-a' -- ${cur}) )
                            return 0
                        fi
                        COMPREPLY=( $(compgen -W '-t -n -bt' -- ${cur}) )
                        return 0
                    ;;
                    'process-mem')
                        COMPREPLY=( $(compgen -W '-t' -- ${cur}) )
                        return 0
                    ;;
                    'topo')
                        COMPREPLY=( $(compgen -W '-t' -- ${cur}) )
                        return 0
                    ;;
                    'node')
                        COMPREPLY=( $(compgen -W '-n' -- ${cur}) )
                        return 0
                    ;;
                    'urma')
                        COMPREPLY=( $(compgen -W '-n -d' -- ${cur}) )
                        return 0
                    ;;
                    'urma-qos')
                        COMPREPLY=( $(compgen -W '-n' -- ${cur}) )
                        return 0
                    ;;
                    '*')
                        return 0
                    ;;
                esac
            ;;

            'import')
                case ${COMP_WORDS[2]} in
                    'cert')
                        COMPREPLY=( $(compgen -W '-s -c -k -l' -- ${cur}) )
                        return 0
                    ;;
                    '*')
                        return 0
                    ;;
                esac
            ;;

            'change')
                case ${COMP_WORDS[2]} in
                    'cert')
                        COMPREPLY=( $(compgen -W '-l' -- ${cur}) )
                        return 0
                    ;;
                    'process-mem')
                        COMPREPLY=( $(compgen -W '-p -n -s -r' -- ${cur}) )
                        return 0
                    ;;
                    '*')
                        return 0
                    ;;
                esac
            ;;

            'remove')
                case ${COMP_WORDS[2]} in
                    'process-mem')
                        COMPREPLY=( $(compgen -W '-p -n' -- ${cur}) )
                        return 0
                    ;;
                    '*')
                        return 0
                    ;;
                esac
            ;;

            'create')
                case ${COMP_WORDS[2]} in
                    'memory')
                        COMPREPLY=( $(compgen -W '-t -l -s -n -r' -- ${cur}) )
                        return 0
                    ;;
                    'urma-qos')
                        COMPREPLY=( $(compgen -W '-p -c -n' -- ${cur}) )
                        return 0
                    ;;
                    '*')
                        return 0
                    ;;
                esac
            ;;

            'delete')
                case ${COMP_WORDS[2]} in
                    'memory')
                        COMPREPLY=( $(compgen -W '-n -t' -- ${cur}) )
                        return 0
                    ;;
                    'urma-qos')
                        COMPREPLY=( $(compgen -W '-n' -- ${cur}) )
                        return 0
                    ;;
                    '*')
                        return 0
                    ;;
                esac
            ;;
            'attach')
                case ${COMP_WORDS[2]} in
                    'memory')
                        COMPREPLY=( $(compgen -W '-n' -- ${cur}) )
                        return 0
                    ;;
                    '*')
                        return 0
                    ;;
                esac
            ;;

            'detach')
                case ${COMP_WORDS[2]} in
                    'memory')
                        COMPREPLY=( $(compgen -W '-n' -- ${cur}) )
                        return 0
                    ;;
                    '*')
                        return 0
                    ;;
                esac
            ;;
            '*')
                return 0
            ;;
        esac
        return 0
    fi

    if [[ ${COMP_WORDS[0]} == *ubsectl ]] && \
           [[ ${COMP_WORDS[1]} == display ]] && \
           [[ ${COMP_WORDS[2]} == memory ]] && \
           [[ "${cmd}" == '--type' || "${cmd}" == '-t' ]]; then

            COMPREPLY=( $(compgen -W 'node_borrow borrow_detail node_lend numa_status config' -- ${cur}) )
            return 0
    fi

    if [[ ${COMP_WORDS[0]} == *ubsectl ]] && \
           [[ ${COMP_WORDS[1]} == display ]] && \
           [[ ${COMP_WORDS[2]} == process-mem ]] && \
           [[ "${cmd}" == '--type' || "${cmd}" == '-t' ]]; then

            COMPREPLY=( $(compgen -W 'config proc_detail' -- ${cur}) )
            return 0
    fi

    if [[ ${COMP_WORDS[0]} == *ubsectl ]] && \
           [[ ${COMP_WORDS[1]} == display ]] && \
           [[ ${COMP_WORDS[2]} == memory ]] && \
           [[ "${cmd}" == '--borrow-type' || "${cmd}" == '-bt' ]]; then

            COMPREPLY=( $(compgen -W 'fd numa share' -- ${cur}) )
            return 0
    fi

    if [[ ${COMP_WORDS[0]} == *ubsectl ]] && \
           [[ ${COMP_WORDS[1]} == display ]] && \
           [[ ${COMP_WORDS[2]} == topo ]] && \
           [[ "${cmd}" == '--type' || "${cmd}" == '-t' ]]; then

            COMPREPLY=( $(compgen -W 'cpu' -- ${cur}) )
            return 0
    fi

    if [[ ${COMP_WORDS[0]} == *ubsectl ]] && \
           [[ ${COMP_WORDS[1]} == delete ]] && \
           [[ ${COMP_WORDS[2]} == memory ]] && \
           [[ "${cmd}" == '--type' || "${cmd}" == '-t' ]]; then

            COMPREPLY=( $(compgen -W 'fd numa share addr' -- ${cur}) )
            return 0
    fi

    if [[ ${COMP_WORDS[0]} == *ubsectl ]] && \
           [[ ${COMP_WORDS[1]} == create ]] && \
           [[ ${COMP_WORDS[2]} == memory ]] && \
           [[ "${cmd}" == '--type' || "${cmd}" == '-t' ]]; then

            COMPREPLY=( $(compgen -W 'numa fd share' -- ${cur}) )
            return 0
    fi

if [[ ${COMP_WORDS[0]} == *ubsectl ]] && \
       [[ ${COMP_WORDS[1]} == create ]] && \
       [[ ${COMP_WORDS[2]} == urma-qos ]] && \
       [[ "${cmd}" == '--pri' || "${cmd}" == '-p' ]]; then

            COMPREPLY=( $(compgen -W '0 1' -- ${cur}) )
            return 0
    fi

    # process-mem: change/remove 的 -p/--pid 补全运行中 PID
    if [[ ${COMP_WORDS[0]} == *ubsectl ]] && \
           [[ ${COMP_WORDS[1]} == change || ${COMP_WORDS[1]} == remove ]] && \
           [[ ${COMP_WORDS[2]} == process-mem ]] && \
           [[ "${cmd}" == '-p' || "${cmd}" == '--pid' ]]; then
            COMPREPLY=( $(_ubse_proc_pids "$cur") )
            return 0
    fi

    # process-mem: change/remove 的 -n/--name 补全运行中进程名
    if [[ ${COMP_WORDS[0]} == *ubsectl ]] && \
           [[ ${COMP_WORDS[1]} == change || ${COMP_WORDS[1]} == remove ]] && \
           [[ ${COMP_WORDS[2]} == process-mem ]] && \
           [[ "${cmd}" == '-n' || "${cmd}" == '--name' ]]; then
            COMPREPLY=( $(_ubse_proc_names "$cur") )
            return 0
    fi

    # process-mem: change 的 -r/--remote-ratio 补全常用比值（对应 -p 0 1 枚举模式）
    if [[ ${COMP_WORDS[0]} == *ubsectl ]] && \
           [[ ${COMP_WORDS[1]} == change ]] && \
           [[ ${COMP_WORDS[2]} == process-mem ]] && \
           [[ "${cmd}" == '-r' || "${cmd}" == '--remote-ratio' ]]; then
            COMPREPLY=( $(compgen -W '0.3 0.5 0.8 1.0' -- ${cur}) )
            return 0
    fi

}

complete -F _ubse_commond_completion ubsectl