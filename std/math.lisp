;;; math.lisp - Numerical and mathematical utilities

(defconst PI  3.141592653589793)
(defconst E   2.718281828459045)
(defconst PHI 1.618033988749895)

(defun 1+ (x)
  (+ x 1))

(defun 1- (x)
  (- x 1))

(defun abs (x)
  (if (< x 0)
      (- 0 x)
      x))

(defun min (a b)
  (if (< a b)
      a
      b))

(defun max (a b)
  (if (> a b)
      a
      b))

(defun clamp (x low high)
  (cond
    ((< x low)  low)
    ((> x high) high)
    (else       x)))

(defun factorial (x)
  (if (= x 0)
      1
      (* x (factorial (1- x)))))
