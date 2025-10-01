# Prints my lost (or drawn) games from a competition-with-transcripts CSV file.
#
# Example usage:
#
# awk -f tools/my-losses.awk competition-results/test-1-competition-350-transcripts.csv

BEGIN {
    FS=","
}

(($4 == "Maks Verver" && $6 != "WIN") ||
 ($7 == "Maks Verver" && $9 != "WIN")) {
    print
}
