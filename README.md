
# alpine_scheduler

Creating a schedule for a round tables event based on participants preferences. First written and  used for the Alpine Conference on Magnetic Resonance in Solids 2019.

## General description

The scenario involves N<sub>P</sub> participants, N<sub>A</sub> of which present abstracts. There are N<sub>S</sub> sessions and in each of them N<sub>R</sub> rooms are used, each with N<sub>C</sub> capacity. During a session, in each room one participant presents their abstract to N<sub>C</sub>-1 other participant ("listeners").

Ahead of the scheduling, participants preferences should be gathered and they are passed to the program as input. The program then tries to find a solution that tries to maximize participants' happiness based on their preferences using a "[Simulated Annealing](https://en.wikipedia.org/wiki/Simulated_annealing)" approach.

The above preferences are a list of scores participants gave a few abstracts of their choosing. Scores can be a number between 1 and 5 ("star rating") given to, for example 20 abstracts that they choose as most interesting to them to hear about.

## Constraints and Considerations
- It's possible to configure maximum presentations per abstract so that no abstract is presented more than e.g. twice
- It's possible to configure range of number of participations, so that each participant may participate in between 10 and 12 sessions - this depends on the number of sessions and capacity of the rooms. E.g. if there are 200 participants and 15 rooms of 10 people, then 150 of 200 people participate in each session and on average each person will participate in 75% of the sessions. for 16 sessions, it's possible to configure range of participations to be 11-13 or 10-14 etc.
- In some cases people may not submit any rankings. In this case the algorithm will prefer to assign them to popular abstracts, otherwise such participants will end up attending all the least popular sessions - rather than all participant being distributed across unpopular sessions.

## Input

The program uses a unique numeric ID for each participant. The input is a [CSV file](https://en.wikipedia.org/wiki/Comma-separated_values) (can be e.g. exported from Microsoft Excel). It should contain 3 columns - participant ID, abstract ID and rating. E.g. if participant 31 gave participant 43's abstract a 4-star rating, the file will include the row "31,43,4". The file can have additional columns and the program will be told which column titles have the required information. Example beginning of input file (the last column can be omitted):

    person_id,abstract_id,rating,person_name
    2,90,5,John A
    2,17,4,John A
    2,19,4,John A
    4,13,3,Bill C
    4,2,3,Bill C
    4,14,5,Bill C
    4,10,1,Bill C
    5,14,5,Jim D
    5,54,5,Jim D
    5,84,5,Jim D
    5,2,2,Jim D

In addition to the above, there are many parameters that can tune the program's behavior.

## Output

The output is a CSV file containing rows of participant IDs. Each row contains N<sub>C</sub> IDs and represents a single room. The first ID in each row is the participant that will be presenting their abstract. There are N<sub>S</sub>•N<sub>R</sub> rows, one for each room and session.

Example beginning of output file:

    90,87,185,114,112,6,2,48,104,178
    76,121,155,146,19,56,160,43,10,109
    142,50,170,25,145,30,16,190,165,62
    83,58,49,34,149,80,130,82,163,26
    138,79,75,65,85,127,118,144,37,115
    120,134,5,123,159,107,36,21,98,137
    175,12,46,174,117,72,152,129,3,179
    192,169,177,29,59,53,89,198,187,68
    1,38,32,167,116,93,24,197,42,44
    74,99,7,67,182,124,70,0,131,125

In addition to the above the program outputs some stats to help evaluate the solution

## Program options:

    -h [ --help ]                         Show help message and exit
    -r [ --ranking_file ] arg             Rankings CSV file
    --results_dir arg (=results)          Directory for saving results
    -i [ --iterations ] arg (=100000)     Number of iterations
    --init_temp arg (=10)                 Initial temperature
    --final_temp arg (=1.0000000000000001e-05)
                                          Final temperature
    --timeslots arg (=18)                 Number of timeslots
    --rooms arg (=9)                      Number of rooms
    --room_size arg (=12)                 Room capacity including speaker
    --person_id_col arg (=person_id)      Name of person_id column
    --abstract_id_col arg (=abstract_id)  Name of abstract_id column
    --score_col arg (=rating)             Name of score column
    --input_delimiter arg (=,)            Delimiter character of input file
    --default_score arg (=0)              Value of empty score
    --max_score arg (=5)                  Minimum value for single score
    --min_score arg (=0)                  Maximum value for single score
    --score_delta arg (=1)                Delta added per score (to avoid 0
                                          score)
    --participation_range arg (=2)        Allowed deviation from mean number of
                                          participations per person
    --max_presentations arg (=3)          Max number of presentations per
                                          abstract
    --seed arg                            Algorithm random seed (for debugging)
    -v [ --verbose ]                      Verbose mode (for debugging)


#### The alpine_scheduler is based on previous work by Lyndon Emsley, Nicolas Giraud and Albert Hoftstetter.
