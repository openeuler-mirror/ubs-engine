#!/bin/bash

#
# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
#

function _ubse_commond_completion() {
    COMPREPLY=()

    local cur=${COMP_WORDS[COMP_CWORD]}
    local cmd=${COMP_WORDS[COMP_CWORD-1]}
    local prev=${COMP_WORDS[COMP_CWORD-2]}

    commands='display import delete check urma'
    display_types='topo memory cluster urma'
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
                COMPREPLY=( $(compgen -W 'cert crl' -- ${cur}) )
                return 0
            ;;
            'delete')
                COMPREPLY=( $(compgen -W 'cert' -- ${cur}) )
                return 0
            ;;
            'check')
                COMPREPLY=( $(compgen -W "${check_types}" -- ${cur}) )
                return 0
            ;;
            'urma')
                COMPREPLY=( $(compgen -W '--node -n' -- ${cur}) )
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
                        COMPREPLY=( $(compgen -W '--type' -- ${cur}) )
                        return 0
                    ;;
                    'urma')
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
                    'crl')
                        COMPREPLY=( $(compgen -W '--ca-crl-file' -- ${cur}) )
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
                        COMPREPLY=( $(compgen -W '-t' -- ${cur}) )
                        return 0
                    ;;
                    'urma')
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
                    'crl')
                        COMPREPLY=( $(compgen -W '-l' -- ${cur}) )
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

            COMPREPLY=( $(compgen -W 'node_borrow borrow_detail' -- ${cur}) )
            return 0
    fi

}

complete -F _ubse_commond_completion ubsectl