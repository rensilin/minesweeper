_minesweeper_comp()
{
	local cur prev opt opts
	COMPREPLY=()
	cur="${COMP_WORDS[COMP_CWORD]}"
	prev="${COMP_WORDS[COMP_CWORD-1]}"
	opt="h v E N H --"
	opts="--help --version --easy --normal --hard"

	if [[ ${cur} == - ]]; then
		COMPREPLY=( $opt )
	elif [[ ${cur} == -* ]] ; then
		COMPREPLY=( $(compgen -W "${opts}" -- ${cur}) )
		return 0
	fi
}
complete -F _minesweeper_comp minesweeper

