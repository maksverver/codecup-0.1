# Calculates my player's results in a competition grouped by opponent.
#
# The output is one comma-separated line per opponent, with the following fields:
#
#   1. opponent
#   2. wins
#   3. draws
#   4. losses
#   5. total games played (2 + 3 + 4)
#   6. total points scored
#   7. points per game (5 / 6)
#
# It's useful to sort on column 4 or 7. Example:
#
# awk -f tools/results-per-opponent.awk \
#       competition-results/test-1-competition-transcripts.csv \
#   | sort -t, -k7 -n
#

BEGIN {
    FS=","
}

function add_game(score, result, opponent) {
    count[opponent] += 1
    scores[opponent] += score
    if (result == "WIN") {
        wins[opponent] += 1
    }
    if (result == "DRAW") {
        draws[opponent] += 1
    }
    if (result == "LOSE") {
        losses[opponent] += 1
    }
}

($4 == "Maks Verver") { add_game($5, $6, $7) }
($7 == "Maks Verver") { add_game($8, $9, $4) }

END {
    for (user in count) {
        printf("%-24s,%3d,%3d,%3d,%3d,%4d,%6.2f\n",
            user,
            wins[user] + 0, draws[user] + 0, losses[user] + 0,
            count[user], scores[user], scores[user] / count[user])
    }
}
