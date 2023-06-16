## Renderer Back Page computation cases - A systematic analysis

The issue: All lines are not of the same height:

- Lines with text in them have a deciline of 10.
- Lines with an end-of-paragraph indicator (empty line) have a deciline of 4.

That results in the complexity of properly computing the start of the previous page when the user wants to return to it.

The following is a systematic analysis of the problem such that an algorithm will be chosen and actions are taken to mitigate the computing issues with special cases when identified.

Two algorithms are considered to resolve the page backward processing. They are both related to the following first steps:

1. Find the location where the end of the back page will be.

2. In succession, go back one paragraph at a time until the 
    number of decilines is greater or equal to the available space on the page.

Then, for algorithm 1:

    3. Starting at the **beginning** of the last paragraph retrieved, go forward one line at a time until the number of decilines remaining is below or equal to the maximum permitted on the page. The next line is the first line to be shown.

and for algorithm 2:

    3. Starting at the *end* of the last paragraph retrieved, go backward until the deciline count is larger than the maximum permitted on the page. The next line is the first line to be shown.

Both algorithms would work if every line were of the same height. That is not the case. Many combinations of text lines and paragraph changes create many cases for which the computation cannot be resolved properly. This causes misalignments, getting a back page with one line more or less than expected.

### Analysis

**Assumption**: There are never cases in which more than one new paragraph will be present in a row. The tokenizer filters these cases.

In the following diagrams, the kind of line(s) present at the beginning/end of pages and the kind of line(s) preceding or following the page are shown as follows:

- **P**: End of paragraph indicator (4 decilines)
- **L**: Normal Line (10 decilines)

The 10 diagrams show the combination of lines that impact the computation on the page location to go to when going backward. 

For each diagram, in this analysis, we consider that a page can contain a maximum of 150 decilines. Below each diagram, there are entries (cases) for each potential deciline count computation done going forward that need to be considered for a fit or not in the page when going backward. Each number is followed with an indicator as follows:

- **-** : Computation does not require any specific action.
- **Y** : Computation *DOES* require specific action.

The analysis is conducted with the two potential algorithms:


### 1. Analysis of Algorithm 1

As this algorithm starts from the top of the last paragraph retrieved, it must go down until there remains a count of decilines less than or equal to 150. The count from the beginning of the paragraph would be a number of 10 decilines (they are all lines) until it reaches the end of the paragraph marker (4 decilines) or 150.

The 10th diagram is the case when the beginning of the last paragraph is equal to 150 as this case will resolve by itself.

The numbers shown below the diagrams are the related cases with the following definitions:

  - Column 1: The number of decilines as computed when going forward
  - Column 2: (In parenthesis) The number of decilines computed up to the start of the last paragraph retrieved
  - Column 3: The cutting value <= 150 as the algorithm goes down one line at a time.

From the analysis, the only cases that seem in trouble are cases related to diagrams E and F, as the end-of-paragraph indicator will be considered part of the page. Some (not) potential solutions:

  - Not a big deal as we can hide them on the page. The problem: this will have a visual impact on the next back page, or subsequent ones. Not functional.

  - The first line appears to be the end-of-paragraph indicator. For the computation, consider it to be a normal line. That would eliminate it from the page. But how not interfere with the diagram C, D, and H that has a legitimate end-of-paragraph indicator at the beginning of the page??

```
|L                |   |L                | <- Start of last
|L                |   |L                |    paragraph
+-----------------+   +-----------------+    retrieved
|L                |   |L                |
|                 |   |                 |
|                 |   |                 |
|                 |   |                 |
|    DIAGRAM A    |   |    DIAGRAM B    |
|                 |   |                 |
|                 |   |                 |
|                 |   |                 |
|                 |   |                 |
|L                |   |P                |
+-----------------+   +-----------------+
|L                |   |L                |

150: -  (170) 150     150: -   (170) 150
148: -  (168) 148     148: -   (168) 148
146: -  (166) 146     146: -   (166) 146
144: -  (164) 144     144: -   (164) 144
142: -  (162) 142     142: -   (162) 142
```
```
|L                |   |L                | <- Start of last
|L                |   |L                |    paragraph
+-----------------+   +-----------------+    retrieved
|P                |   |P                |
|L                |   |L                |
|                 |   |                 |
|                 |   |                 |
|    DIAGRAM C    |   |    DIAGRAM D    |
|                 |   |                 |
|                 |   |                 |
|                 |   |                 |
|                 |   |                 |
|L                |   |P                |
+-----------------+   +-----------------+
|L                |   |L                |

150: -  (170) 150     150: -  (170) 150
148: -  (168) 148     148: -  (168) 148
146: -  (166) 146     146: -  (166) 146
144: -  (164) 144     144: -  (164) 144
142: -  (162) 142     142: -  (162) 142
```
```
|L                |   |L                | <- Start of last 
|P                |   |P                |    paragraph
+-----------------+   +-----------------+    retrieved
|L                |   |L                |
|                 |   |                 |
|                 |   |                 |  These 2 cases  
|                 |   |                 |  only possible  
|    DIAGRAM E    |   |    DIAGRAM F    |  if decilines is
|                 |   |                 |  not 150        
|                 |   |                 |
|                 |   |                 |
|                 |   |                 |
|L                |   |P                |
+-----------------+   +-----------------+
|L                |   |L                |

148: -  (162) 148     148: -  (162) 148          
146: Y  (160) 150     146: Y  (160) 150
144: Y  (158) 148     144: Y  (158) 148
142: Y  (156) 146     142: Y  (156) 146
```
```
|L                |   |L                | <- Start of last
|L                |   |L                |    paragraph
+-----------------+   +-----------------+    retrieved
|L                |   |P                |
|P                |   |                 |
|                 |   |                 |
|                 |   |                 |
|    DIAGRAM G    |   |    DIAGRAM H    |
|                 |   |                 |
|                 |   |                 |
|                 |   |                 |
|                 |   |                 |
|L                |   |L                |
+-----------------+   +-----------------+
|P                |   |P                |

150: -  (170) 150     150: -  (170) 150
148: -  (168) 148     148: -  (168) 148
```
```
|L                | <- Start of last
|P                |    paragraph
+-----------------+    retrieved
|L                |
|                 |
|                 |
|                 |
|    DIAGRAM I    |
|                 |
|                 |
|                 |
|                 |
|L                |
+-----------------+
|P                |

148: -  (162) 148
```
```
+-----------------+   
|L                |
|L                |
|L                |
|L                |  
|.   DIAGRAM J    |  
|.                |  
|.                |  
|L                |  
|L                |  
|L                |  
+-----------------+  
|L                |  

150: -  (150) 150  
```

### 2. Analysis of algorithm 2

As this algorithm start from the bottom of the last paragraph retrieved, it must go up until it reaches the point where the number of decilines is larger than the maximum decilines permitted (here 150). The previous line is the one that will start the page.

The paragraph may end with an end-of-paragraph indicator or not, depending if the first paragraph to retrieve was already part of the following page and uses at least 150 decilines in length. If there is no end-of-paragraph indicator, the case is easy: the algorithm will stop once 150 decilines is reached as every line is 10 decilines. So this case is not further analysed (DIAGRAM J).

All the diagrams A to I consider that there is an end-of-paragraph indicator related to
the last paragraph retrieved to have at least 150 decilines of text to work with.

The numbers shown below the diagrams are the related cases with the following definitions:

  - Column 1: The number of decilines as computed when going forward
  - Column 2: (In parenthesis) The number of decilines computed up to end of the last paragraph retrieved (before the end-of-paragraph indicator)
  - Column 3: The cutting value <= 150 as the algorithm goes up one line at a time and, once the decilines is greather than 150, takes the location of the last line checked before.

From the analysis of all cases, there is no special case to consider. This seems to 
be the ideal algorithm to implement.

Stay tuned!

```
|L                |   |L                |
|L                |   |L                |
+-----------------+   +-----------------+
|L                |   |L                |
|P                |   |P                |
|                 |   |                 |
|                 |   |                 |
|    DIAGRAM A    |   |    DIAGRAM B    |
|                 |   |                 |
|                 |   |                 |
|                 |   |                 |
|                 |   |                 |
|L                |   |P                |
+-----------------+   +-----------------+
|L                |   |L                |

150: - (136) 150      150: - (136) 150
148: - (134) 148      148: - (134) 148
146: - (132) 146      146: - (132) 146
144: - (130) 144      144: - (130) 144
142: - (128) 142      142: - (128) 142
```
```
|L                |   |L                | <- Start of last 
|L                |   |L                |    paragraph 
+-----------------+   +-----------------+    retrieved
|P                |   |P                | 
|L                |   |L                |
|                 |   |                 |
|                 |   |                 |
|    DIAGRAM C    |   |    DIAGRAM D    |
|                 |   |                 |
|                 |   |                 |
|                 |   |                 |
|                 |   |                 |
|L                |   |P                |
+-----------------+   +-----------------+
|L                |   |L                |

150: - (146) 150      150: - (146) 150
148: - (144) 148      148: - (144) 148
146: - (142) 142      146: - (142) 142
144: - (140) 144      144: - (140) 144
142: - (138) 142      142: - (138) 142
```
```
|L                |   |L                |
|P                |   |P                | 
+-----------------+   +-----------------+
|L                |   |L                | These 2 cases  
|                 |   |                 | only possible  
|                 |   |                 | if decilines is
|                 |   |                 | not 150        
|    DIAGRAM E    |   |    DIAGRAM F    |
|                 |   |                 |
|                 |   |                 |
|                 |   |                 |
|                 |   |                 |
|L                |   |P                |
+-----------------+   +-----------------+
|L                |   |L                |

148: - (148) 148      148: - (148) 148
146: - (146) 146      146: - (146) 146
144: - (144) 144      144: - (144) 144
142: - (142) 142      142: - (142) 142
```
```
|L                |   |L                |
|L                |   |L                |
+-----------------+   +-----------------+
|L                |   |P                |
|P                |   |L                |
|                 |   |                 |
|                 |   |                 |
|    DIAGRAM G    |   |    DIAGRAM H    |
|                 |   |                 |
|                 |   |                 |
|                 |   |                 |
|                 |   |                 |
|L                |   |L                |
+-----------------+   +-----------------+
|P                |   |P                |

150: - (136) 150      150: - (146) 150
148: - (134) 134      148: - (144) 148
```
```
|L                |
|P                |
+-----------------+
|L                |
|                 |
|                 |
|                 |
|    DIAGRAM I    |
|                 |
|                 |
|                 |
|                 |
|L                |
+-----------------+
|P                |

148: - (148) 148
```
```
+-----------------+   
|L                | 
|L                |
|L                |
|L                |  
|.   DIAGRAM J    |  
|.                |  
|.                |  
|L                |  
|L                |  
|L                |  
+-----------------+  
|L                |  

150: - (0) 150
```