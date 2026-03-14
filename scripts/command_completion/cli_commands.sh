#!/bin/bash

#
# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
#

function _ubse_commond_completion() {
    COMPREPLY=()

    local cur=${COMP_WORDS[COMP_CWORD]}
    local cmd=${COMP_WORDS[COMP_CWORD-1]}
    local prev=${COMP_WORDS[COMP_CWORD-2]}

    commands='create display import delete check change remove detach attach urma'
    display_types='topo memory cluster cert node urma'
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
            'import'|'change'|'remove')
                COMPREPLY=( $(compgen -W 'cert' -- ${cur}) )
                return 0
            ;;
            'check')
                COMPREPLY=( $(compgen -W "${check_types}" -- ${cur}) )
                return 0
            ;;
            'delete'|'create'|'detach'|'attach')
                COMPREPLY=( $(compgen -W 'memory' -- ${cur}) )
                return 0
            ;;
            'urma')
                COMPREPLY=( $(compgen -W '--node -n --dev -d' -- ${cur}) )
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
                        COMPREPLY=( $(compgen -W '--type --borrow-type --name' -- ${cur}) )
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
                    '*')
                        return 0
                    ;;
                esac
            ;;

            'create')
                case ${COMP_WORDS[2]} in
                    'memory')
                        COMPREPLY=( $(compgen -W '--type --link --size --name --region' -- ${cur}) )
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
                        COMPREPLY=( $(compgen -W '-t -n -bt' -- ${cur}) )
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


}

complete -F _ubse_commond_completion ubsectl