;;; primes.lisp - sieve of Eratosthenes over a growable vector.
;;;
;;; The flags live in a vector, which is mutable by reference, so the inner
;;; loop marks multiples in place. Note that a function parameter is a deep
;;; copy of the argument, so the marking happens in the same function that
;;; owns the vector instead of in a helper that receives it.
;;;
;;; Run it with: ./build/glisp examples/primes.lisp

(defun make-flags (limit)
  "Returns a vector of LIMIT true flags."
  (let ((flags {}))
    (dotimes (i limit)
      (push flags t))
    flags))

(defun primes-below (limit)
  "Returns a list of the primes below LIMIT."
  (let ((flags (make-flags limit))
        (found nil))
    (set-at flags 0 nil)
    (set-at flags 1 nil)
    (let ((candidate 2))
      (while (< candidate limit)
        (if (get flags candidate)
            (progn
              (set found (cons candidate found))
              ;; Mark every multiple of CANDIDATE, starting from its square.
              (let ((multiple (* candidate candidate)))
                (while (< multiple limit)
                  (set-at flags multiple nil)
                  (set multiple (+ multiple candidate))))))
        (set candidate (+ candidate 1))))
    (reverse found)))

(set result (primes-below 50))
(println result)
(println (format "{} primes below 50" (len result)))
(assert (= (len result) 15))
(assert (= (car result) 2))
(assert (= (last result) 47))
(println "primes ok")
