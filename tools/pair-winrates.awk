#!/usr/bin/awk -f

# Run this on a competition results.txt file to calculate the winrate for
# each pair of players.
#
# It will print a table like this:
#
#             foo       bar        baz
#  foo          -  75% (5%)  83% (10%)
#  bar   20% (5%)         -   42% (1%)
#  baz    7% (5%)  56% (1%)          -
#
# which means that `foo` won 75% of its matches against `bar`,
# and tied 5%, and (therefore) lost the remaining 20%.
#
# This script currently does not distinguish between player 1 and player 2
# victories (which could be interesting, too).

BEGIN {
    expected_header = "Game Player 1           Player 2           Sc1 Sc2 Outc1 Outc2 Pts1 Pts2 Time 1 Time 2"
    player_count = 0;
    total_matches = 0;
}

NR == 1 && $0 != expected_header {
    print "Invalid header row!"
    print "Expected", expected_header
    print "Received", $0
    exit 1
}

function registerPlayer(name) {
    if (!(name in player_index)) {
        player_index[name] = player_count;
        player_names[player_count] = name;
        player_count += 1;
    }
    return player_index[name];
}

$6 == "WIN" || $7 == "WIN" || ($6 == "TIE" && $7 == "TIE") {
    player1 = $2;
    player2 = $3;
    registerPlayer(player1);
    registerPlayer(player2);
    matches[player1 "/" player2] += 1;
    matches[player2 "/" player1] += 1;
    if ($6 == "WIN") { wins[player1 "/" player2] += 1 }
    if ($7 == "WIN") { wins[player2 "/" player1] += 1 }
    total_matches += 1;
}

END {
    # Print total number of matches (to verify all lines were processed)
    print(total_matches, "total matches")

    # Print column header
    printf("%20s ", "");
    for (j = 0; j < player_count; j += 1) {
        loser = player_names[j];
        printf("%20s ", player_names[j]);
    }
    printf("\n");
    # Print rows
    for (i = 0; i < player_count; i += 1) {
        winner = player_names[i];
        printf("%20s ", winner);
        for (j = 0; j < player_count; j += 1) {
            loser = player_names[j];
            m = matches[winner "/" loser]
            if (m == 0) {
                s = "N/A"
            } else {
                w = wins[winner "/" loser]
                l = wins[loser "/" winner]
                t = m - w - l
                s = sprintf("%.2f%% (%.2f%%)", 100 * (w / m), 100 * (t / m))
            }
            printf("%20s ", s)
        }
        printf("\n");
    }
}
