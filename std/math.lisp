;;; math.lisp - Numerical and mathematical utilities

(defconst PI  3.141592653589793)
(defconst E   2.718281828459045)
(defconst PHI 1.618033988749895)

(defun 1+ (x)
  (assert (number? x) "X must be number")
  (+ x 1))

(defun 1- (x)
  (assert (number? x) "X must be number")
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
  (assert (and (number? x) (>= x 0)) "X must be non-negative number")
  (if (= x 0)
      1
      (* x (factorial (1- x)))))

(defun pow (x power)
  (assert (and (number? x) (number? power)) "X and POWER must be numbers")
  (assert (not (= x power 0)) "Cannot raise 0 to 0th power")
  (exp (* power (log x))))

(defun cbrt (x)
  (assert (number? x) "X must be number")
  (copysign (pow (abs x) (/ 1.0 3.0)) x))

(defun tan (x)
  (assert (number? x) "X must be number")
  (/ (sin x) (cos x)))

(defun cot (x)
  (assert (number? x) "X must be number")
  (/ 1.0 (tan x)))

(defconst tg tan)
(defconst ctg cot)

(defun sec (x)
  (assert (number? x) "X must be number")
  (/ 1.0 (cos x)))

(defun csc (x)
  (assert (number? x) "X must be number")
  (/ 1.0 (sin x)))

(defun asin (x)
  (assert (and (number? x) (<= (abs x) 1.0)) "X must be in range [-1, 1]")
  (atan2 x (sqrt (- 1.0 (* x x)))))

(defun acos (x)
  (assert (and (number? x) (<= (abs x) 1.0)) "X must be in range [-1, 1]")
  (atan2 (sqrt (- 1.0 (* x x))) x))

(defun atan (x)
  (assert (number? x) "X must be number")
  (atan2 x 1.0))
