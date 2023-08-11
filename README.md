# Altbat Programming Language

The Altbat programming language created for my personal learning purposes in the C programming language.
 Inspired by the simplicity of Lisp, Altbat was designed to allow me, the developer, to better understand
 the fundamentals of programming in C.

## Features

Altbat has the following features:
- **Arithmetic Operators**
- **Ternary Operators**
- **Variables**
- **Functions**
- **Conditional Structures**

## Usage Example
Here's a simple example in Altbat

```lisp
(def {a b} 2 1) // Multi Vars declaration

(def {calc-amor} (\{x y} { // Defining function
        (if (== (- x 1) y) { // If x-1 equals y
            print "I love bats"

            (if (== (* x y) 2) { // If x*y equals 2
                print "I love bats doubly"
            } {})
        } { // Else
            print "No love for bats"
        })
    })
)

(calc-bat a b) // Call function with a, b paramters
```

# Compile and Run
To compile `main.c`, use `gcc`:
```sh
cc main.c -std=c99 -Wall -ledit -lm libs/mpc/mpc.c -o altbat
```

Then run the executable:
```sh
./altbat
```


Alternatively, you can compile and run using the provided `run.sh` script:
```sh
bash run.sh
```

In the prompt, you can test any feature, similar to the interactive prompts of Python or Node.js, for example.<br>
You can run a file by executing this command in Altbat's prompt:
```js
require "filename"
```

Or using the compiled file:
```sh
./altbat filename
```


# Note
Keep in mind that Altbat is in an early stage of development and is intended solely for study purposes.
 It does not have all the advanced features of complete programming languages.
 It's important to note that I don't plan to continue with this language, instead,
 I aim to create a new and vastly improved language using the insights gained from working on Altbat.


# Credits
Altbat utilizes a library called [`mpc`](https://github.com/orangeduck/mpc)

I learned a lot from the tutorial available at [Build Your Own Lisp](https://buildyourownlisp.com/),
 which greatly influenced the development of Altbat.
