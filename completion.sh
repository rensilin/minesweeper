_minesweeper_comp()
{
	local cur opt opts
	COMPREPLY=()
	cur="${COMP_WORDS[COMP_CWORD]}"
	opt="h v E N H --"
	opts="--help --version --easy --normal --hard --no-max-size --show --seed --first"

	if [[ ${cur} == - ]]; then
		COMPREPLY=( $opt )
	elif [[ ${cur} == -* ]] ; then
		COMPREPLY=( $(compgen -W "${opts}" -- ${cur}) )
		return 0
	fi
}
complete -F _minesweeper_comp minesweeper

