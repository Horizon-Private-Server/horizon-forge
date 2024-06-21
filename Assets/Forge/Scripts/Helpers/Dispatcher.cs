using System.Collections.Generic;
using System.Threading;
using System;
using UnityEngine;

public class Dispatcher : MonoBehaviour
{
    static Dispatcher _instance;
    static volatile bool _queued = false;
    static List<Action> _backlog = new List<Action>(8);
    static List<Action> _actions = new List<Action>(8);

    static volatile bool _coQueued = false;
    static List<System.Collections.IEnumerator> _coBacklog = new List<System.Collections.IEnumerator>(8);
    static List<System.Collections.IEnumerator> _coroutines = new List<System.Collections.IEnumerator>(8);

    public static void RunAsync(Action action)
    {
        ThreadPool.QueueUserWorkItem(o => action());
    }

    public static void RunAsync(Action<object> action, object state)
    {
        ThreadPool.QueueUserWorkItem(o => action(o), state);
    }

    public static void RunOnMainThread(Action action)
    {
        lock (_backlog)
        {
            _backlog.Add(action);
            _queued = true;
        }
    }

    public static void StartCoroutineOnMainThread(System.Collections.IEnumerator routine)
    {
        lock (_coBacklog)
        {
            _coBacklog.Add(routine);
            _coQueued = true;
        }
    }

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
    private static void Initialize()
    {
        if (_instance == null)
        {
            _instance = new GameObject("Dispatcher").AddComponent<Dispatcher>();
            DontDestroyOnLoad(_instance.gameObject);
        }
    }

    private void Update()
    {
        if (_queued)
        {
            lock (_backlog)
            {
                var tmp = _actions;
                _actions = _backlog;
                _backlog = tmp;
                _queued = false;
            }

            foreach (var action in _actions)
                action();


            _actions.Clear();
        }

        if (_coQueued)
        {
            lock (_coBacklog)
            {
                var tmp = _coroutines;
                _coroutines = _coBacklog;
                _coBacklog = tmp;
                _coQueued = false;
            }

            foreach (var routine in _coroutines)
            {
                StartCoroutine(routine);
            }

            _coroutines.Clear();
        }
    }
}